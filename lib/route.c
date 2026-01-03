/**
 * @file route.c
 * @author kioz.wang (never.had@outlook.com)
 * @brief
 * @version 0.1
 * @date 2025-12-04
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

#include "route.h"
#include "global.h"
#include "logger/logger.h"
#include "misc.h"
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

route_item_t *route_item_create(const storage_ctx_t *storage, uint32_t num_prefix, const char *prefix[]) {
    route_item_t *item = malloc(sizeof(route_item_t));
    if (!item) return NULL;
    if (!(item->prefix = arraydup_cstring(prefix, num_prefix))) {
        free(item);
        return NULL;
    };
    item->storage = *storage;
    item->nref    = 0;
    return item;
}

void route_item_destroy(route_item_t *item) {
    if (!item) return;
    arrayfree_cstring(item->prefix);
    storage_destructor(&item->storage);
    free(item);
}

struct route {
    struct route_list list;
    pthread_rwlock_t  rwlock;
};
typedef struct route route_t;

void *route_create(void) {
    route_t *route = malloc(sizeof(route_t));
    if (!route) return NULL;

    LIST_INIT(&route->list);

    errno = pthread_rwlock_init(&route->rwlock, NULL);
    if (errno) {
        free(route);
        return NULL;
    }
    return route;
    logfI("[route] created");
}

void route_destroy(void *_route) {
    route_t *route = _route;
    if (!route) return;

    pthread_rwlock_rdlock(&route->rwlock);
    route_item_t *temp = NULL;
    LIST_FOREACH(temp, &route->list, entry) { logfE("[route] remain %s", temp->storage.name); }
    assert(LIST_EMPTY(&route->list)); /* TODO ? */
    pthread_rwlock_unlock(&route->rwlock);

    pthread_rwlock_destroy(&route->rwlock);
    free(route);
    logfI("[route] destroyed");
}

void route_init(void *_route, struct route_list list) {
    route_t *route = _route;

    route->list.lh_first = list.lh_first;
    if (list.lh_first) {
        list.lh_first->entry.le_prev = &route->list.lh_first;
        // list.lh_first = NULL;
    }
}

int __route_register(struct route_list *list, const storage_ctx_t *storage, uint32_t num_prefix, const char *prefix[]) {
    route_item_t *temp = NULL;
    LIST_FOREACH(temp, list, entry) {
        if (!strcmp(temp->storage.name, storage->name)) {
            logfE("[route] register %s but name occupied", storage->name);
            return EEXIST;
        }
    }

    route_item_t *item = route_item_create(storage, num_prefix, prefix);
    if (!item) {
        logfE("[route] register %s but fail to create item", storage->name);
        return errno;
    }

    LIST_INSERT_HEAD(list, item, entry);
    logfI("[route] register %s", storage->name);
    return 0;
}

int route_register(void *_route, const storage_ctx_t *storage, uint32_t num_prefix, const char *prefix[]) {
    route_t *route = _route;
    pthread_rwlock_wrlock(&route->rwlock);
    int ret = __route_register(&route->list, storage, num_prefix, prefix);
    pthread_rwlock_unlock(&route->rwlock);
    return ret;
}

int __route_unregister(struct route_list *list, const char *name) {
    route_item_t *item = NULL;

    LIST_FOREACH(item, list, entry) {
        if (!strcmp(item->storage.name, name)) {
            break;
        }
    }
    if (!item) {
        logfE("[route] unregister %s but not found", name);
        return ENOENT;
    }

    int nref = atomic_load(&item->nref);
    if (nref) {
        logfE("[route] unregister %s but busy (%d refs)", item->storage.name, nref);
        return EBUSY;
    }

    LIST_REMOVE(item, entry);
    logfI("[route] unregister %s", item->storage.name);
    route_item_destroy(item);
    return 0;
}

int route_unregister(void *_route, const char *name) {
    route_t *route = _route;
    int      ret   = 0;

    pthread_rwlock_wrlock(&route->rwlock);

    if (name) {
        ret = __route_unregister(&route->list, name);
    } else {
        while (!LIST_EMPTY(&route->list) && !ret) {
            ret = __route_unregister(&route->list, LIST_FIRST(&route->list)->storage.name);
        }
    }

    pthread_rwlock_unlock(&route->rwlock);
    return ret;
}

int route_match(void *_route, const char *key, const storage_ctx_t **storage) {
    route_t      *route = _route;
    int           ret   = 0;
    route_item_t *item  = NULL;

    pthread_rwlock_rdlock(&route->rwlock);

    LIST_FOREACH(item, &route->list, entry) {
        for (int i = 0; item->prefix[i]; i++) {
            if (prefix_match(item->prefix[i], key)) {
                logfV("[route] " logFmtKey " match " logFmtKey " of %s", key, item->prefix[i], item->storage.name);
                if (storage) {
                    atomic_fetch_add(&item->nref, 1);
                    *storage = &item->storage;
                }
                goto exit;
            }
        }
    }
    ret = ENOENT;
    logfE("[route] " logFmtKey " match nothing", key);

exit:
    pthread_rwlock_unlock(&route->rwlock);
    return ret;
}

void route_deref(const storage_ctx_t *storage) {
    route_item_t *item = (route_item_t *)((char *)storage - offsetof(route_item_t, storage));
    atomic_fetch_sub(&item->nref, 1);
}
