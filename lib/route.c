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
#include "misc.h"
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>
#include <unistd.h>

struct route_item {
    storage_t    storage; /* Note: cannot be a pointer, see `route_deref` */
    const char **prefix;
    atomic_int   nref;
    LIST_ENTRY(route_item) entry;
};
typedef struct route_item route_item_t;

void *route_item_create(const storage_t *storage, uint32_t num_prefix, const char *prefix[]) {
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

void route_item_destroy(void *_item) {
    route_item_t *item = _item;
    if (!item) return;
    arrayfree_cstring(item->prefix);
    prop_storage_destructor(&item->storage);
    free(item);
}

LIST_HEAD(route_list, route_item);

void *route_list_create(void) {
    struct route_list *list = malloc(sizeof(struct route_list));
    if (!list) return NULL;
    LIST_INIT(list);
    return list;
}

void route_list_destroy(void *_list) {
    struct route_list *list = _list;
    if (!list) return;
    while (!LIST_EMPTY(list)) {
        route_item_t *item = LIST_FIRST(list);
        LIST_REMOVE(item, entry);
        route_item_destroy(item);
    }
    // TODO warning, unregister all
    // LIST_FOREACH(temp, list, entry) { logfE("[route] remain %s", temp->storage.name); }
    // assert(LIST_EMPTY(list)); /* TODO ? */
    free(list);
}

int route_list_register(void *_list, const storage_t *storage, uint32_t num_prefix, const char *prefix[]) {
    struct route_list *list = _list;
    route_item_t      *temp = NULL;

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

int route_list_unregister(void *_list, const char *name) {
    struct route_list *list = _list;
    route_item_t      *item = NULL;

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

struct route {
    void            *list;
    pthread_rwlock_t rwlock;
};
typedef struct route route_t;

void *route_create(void *_init_list) {
    route_t *route = malloc(sizeof(route_t));
    if (!route) return NULL;

    if (_init_list) route->list = _init_list;
    else {
        route->list = route_list_create();
        if (!route->list) {
            free(route);
            return NULL;
        }
    }

    errno = pthread_rwlock_init(&route->rwlock, NULL);
    if (errno) {
        free(route->list);
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
    route_list_destroy(route->list);
    pthread_rwlock_unlock(&route->rwlock);

    pthread_rwlock_destroy(&route->rwlock);
    free(route);
    logfI("[route] destroyed");
}

int route_register(void *_route, const storage_t *storage, uint32_t num_prefix, const char *prefix[]) {
    route_t *route = _route;
    pthread_rwlock_wrlock(&route->rwlock);
    int ret = route_list_register(route->list, storage, num_prefix, prefix);
    pthread_rwlock_unlock(&route->rwlock);
    return ret;
}

int route_unregister(void *_route, const char *name) {
    route_t *route = _route;
    int      ret   = 0;

    pthread_rwlock_wrlock(&route->rwlock);
    ret = route_list_unregister(route->list, name);
    pthread_rwlock_unlock(&route->rwlock);
    return ret;
}

int route_match(void *_route, const char *key, const storage_t **storage) {
    route_t      *route = _route;
    int           ret   = 0;
    route_item_t *item  = NULL;

    pthread_rwlock_rdlock(&route->rwlock);

    LIST_FOREACH(item, (struct route_list *)route->list, entry) {
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

void route_deref(const storage_t *storage) {
    route_item_t *item = (route_item_t *)((char *)storage - offsetof(route_item_t, storage));
    atomic_fetch_sub(&item->nref, 1);
}
