/**
 * @file named_mutex.c
 * @author kioz.wang (never.had@outlook.com)
 * @brief
 * @version 0.1
 * @date 2025-12-11
 *
 * @copyright MIT License
 *
 *  Copyright (c) 2025 kioz.wang
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 */

#include "named_mutex.h"
#include "tree.h"
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

struct named_mutex {
    const char     *name;
    pthread_mutex_t mutex;
    int             nref;
    RB_ENTRY(named_mutex) entry;
};
typedef struct named_mutex nmtx_t;

struct named_mutex_namespace {
    RB_HEAD(nmtx_tree, named_mutex) tree;
    pthread_mutex_t mutex;
};
typedef struct named_mutex_namespace nmtx_namespace_t;

#if !defined(__uintptr_t_defined) && !defined(__uintptr_t)
#if defined(__LP64__) || defined(_LP64)
typedef unsigned long __uintptr_t;
#else
typedef unsigned int __uintptr_t;
#endif
#define __uintptr_t_defined 1
#endif

#define __unused __attribute__((unused))

static int cmp(nmtx_t *a, nmtx_t *b) { return strcmp(a->name, b->name); }

RB_GENERATE_STATIC(nmtx_tree, named_mutex, entry, cmp);

static nmtx_t *named_mutex_create(const char *name) {
    nmtx_t *nmtx = (nmtx_t *)calloc(1, sizeof(nmtx_t));
    if (!nmtx) return NULL;
    if (!(nmtx->name = strdup(name))) {
        free(nmtx);
        return NULL;
    }
    pthread_mutex_init(&nmtx->mutex, NULL);
    return nmtx;
}

static void named_mutex_destroy(nmtx_t *nmtx) {
    if (!nmtx) return;
    assert(nmtx->nref == 0);
    pthread_mutex_destroy(&nmtx->mutex);
    free((void *)nmtx->name);
    free(nmtx);
}

void *named_mutex_create_namespace(void) {
    nmtx_namespace_t *ns = (nmtx_namespace_t *)malloc(sizeof(nmtx_namespace_t));
    if (!ns) return NULL;

    RB_INIT(&ns->tree);
    pthread_mutex_init(&ns->mutex, NULL);
    return ns;
}

void named_mutex_destroy_namespace(void *_ns) {
    if (!_ns) return;
    nmtx_namespace_t *ns = _ns;

    pthread_mutex_lock(&ns->mutex);
    assert(RB_EMPTY(&ns->tree));
    pthread_mutex_unlock(&ns->mutex);

    pthread_mutex_destroy(&ns->mutex); /* TODO EBUSY */
    free(ns);
}

int named_mutex_lock(void *_ns, const char *name) {
    nmtx_t *nmtx = named_mutex_create(name);
    if (!nmtx) return errno;
    nmtx_namespace_t *ns = _ns;

    pthread_mutex_lock(&ns->mutex);
    nmtx_t *old_item = RB_INSERT(nmtx_tree, &ns->tree, nmtx);
    if (old_item) {
        named_mutex_destroy(nmtx);
        nmtx = old_item;
    }
    nmtx->nref++;
    pthread_mutex_unlock(&ns->mutex);

    pthread_mutex_lock(&nmtx->mutex);
    return 0;
}

int named_mutex_unlock(void *_ns, const char *name) {
    nmtx_t           *nmtx   = NULL;
    nmtx_t            shadow = {.name = name};
    nmtx_namespace_t *ns     = _ns;

    pthread_mutex_lock(&ns->mutex);
    nmtx = RB_FIND(nmtx_tree, &ns->tree, &shadow);
    if (!nmtx) {
        pthread_mutex_unlock(&ns->mutex);
        return ENOENT;
    }
    pthread_mutex_unlock(&ns->mutex);

    pthread_mutex_unlock(&nmtx->mutex);

    pthread_mutex_lock(&ns->mutex);
    if (--nmtx->nref == 0) {
        RB_REMOVE(nmtx_tree, &ns->tree, nmtx);
        named_mutex_destroy(nmtx);
    }
    pthread_mutex_unlock(&ns->mutex);
    return 0;
}
