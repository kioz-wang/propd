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
#include "logger/logger.h"
#include "misc.h"
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

route_item_t *route_item_create(io_t io, const char *name, uint32_t num_prefix, const char *prefix[]) {
    route_item_t *item = (route_item_t *)malloc(sizeof(route_item_t));
    if (!item) goto exit;
    item->io = io;
    if (!(item->name = strdup(name))) goto exit;
    if (!(item->prefix = arraydup_cstring(prefix, num_prefix))) goto exit;
    return item;
exit:
    route_item_destroy(item);
    return NULL;
}

void route_item_destroy(route_item_t *item) {
    if (item) {
        free((void *)item->name);
        arrayfree_cstring(item->prefix);
        io_deinit(&item->io);
        free(item);
    }
}

route_t *route_create(void) {
    route_t *route = (route_t *)malloc(sizeof(route_t));
    if (!route) return NULL;
    int ret = 0;

    LIST_INIT(&route->list);

    ret = pthread_rwlock_init(&route->rwlock, NULL);
    if (ret) {
        logfE("[route] fail to pthread_rwlock_init" logFmtRet, ret);
        free(route);
        return NULL;
    }
    logfI("[route] created");
    return route;
}

void route_destroy(route_t *route) {
    if (!route) return;

    pthread_rwlock_rdlock(&route->rwlock);
    assert(LIST_EMPTY(&route->list)); /* TODO ? */
    pthread_rwlock_unlock(&route->rwlock);

    pthread_rwlock_destroy(&route->rwlock);
    free(route);
    logfI("[route] destroyed");
}

int route_register(route_t *route, io_t io, const char *name, uint32_t num_prefix, const char *prefix[]) {
    route_item_t *item = route_item_create(io, name, num_prefix, prefix);
    if (!item) {
        logfE("[route] <%s> fail to allocate item" logFmtErrno, name, logArgErrno);
        return ENOMEM;
    }
    int ret = 0;

    pthread_rwlock_wrlock(&route->rwlock);

    route_item_t *temp = NULL;
    LIST_FOREACH(temp, &route->list, entry) {
        if (!strcmp(temp->name, name)) {
            logfE("[route] <%s> item name occupied", name);
            ret = EEXIST;
            goto exit;
        }
    }

    LIST_INSERT_HEAD(&route->list, item, entry);

    logfI("[route] <%s> register item", name);

exit:
    pthread_rwlock_unlock(&route->rwlock);
    if (ret) route_item_destroy(item);
    return ret;
}

int route_unregister(route_t *route, const char *name) {
    route_item_t *item = NULL;
    int           ret  = 0;

    pthread_rwlock_wrlock(&route->rwlock);

    if (name) {
        LIST_FOREACH(item, &route->list, entry) {
            if (!strcmp(item->name, name)) {
                break;
            }
        }
        if (!item) {
            logfE("[route] <%s> item not found", name);
            ret = ENOENT;
            goto exit;
        }
    } else {
        if (LIST_EMPTY(&route->list)) {
            logfW("[route] <?> no item anymore");
            ret = ENOENT;
            goto exit;
        }
        item = LIST_FIRST(&route->list);
    }
    LIST_REMOVE(item, entry);

    logfI("[route] <%s> unregister %sitem", item->name, name ? "" : "first ");

exit:
    pthread_rwlock_unlock(&route->rwlock);
    route_item_destroy(item);
    return ret;
}

int route_match(route_t *route, const char *key, io_t *io) {
    route_item_t *item = NULL;
    int           ret  = 0;

    pthread_rwlock_rdlock(&route->rwlock);

    LIST_FOREACH(item, &route->list, entry) {
        for (int i = 0; item->prefix[i]; i++) {
            if (prefix_match(item->prefix[i], key)) {
                logfV("[route] <%s> match <%s> using %s", item->name, key, item->prefix[i]);
                if (io) *io = item->io;
                goto exit;
            }
        }
    }
    logfW("[route] <?> no match for <%s>", key);
    ret = ENOENT;

exit:
    pthread_rwlock_unlock(&route->rwlock);
    return ret;
}
