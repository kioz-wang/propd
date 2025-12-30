/**
 * @file route.h
 * @author kioz.wang (never.had@outlook.com)
 * @brief
 * @version 0.1
 * @date 2025-11-18
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

#ifndef __PROPD_ROUTE_H
#define __PROPD_ROUTE_H

#include "storage.h"
#include <stdatomic.h>
#include <stdint.h>
#include <sys/queue.h>

struct route_item {
    storage_ctx_t storage; /* Note: cannot be a pointer, see `route_deref` */
    const char  **prefix;
    atomic_int    nref;
    LIST_ENTRY(route_item) entry;
};
typedef struct route_item route_item_t;

LIST_HEAD(route_list, route_item);

/**
 * @brief Allocate and initialize an item of route
 *
 * @param storage
 * @param num_prefix (ref. length of arraydup_cstring)
 * @param prefix (ref. array of arraydup_cstring)
 * @return route_item_t* On error, return NULL and set errno
 */
route_item_t *route_item_create(const storage_ctx_t *storage, uint32_t num_prefix, const char *prefix[]);
/**
 * @brief Release an item of route
 *
 * @param item maybe null
 */
void route_item_destroy(route_item_t *item);

/**
 * @brief Allocate and initialize a route
 *
 * @return void* 路由表对象（On error, return NULL and set errno）
 */
void *route_create(void);
/**
 * @brief Release a route
 *
 * @param route 路由表对象 (maybe null)
 */
void route_destroy(void *route);
/**
 * @brief Initialize route
 *
 * @param route
 * @param list
 */
void route_init(void *route, struct route_list list);

int __route_register(struct route_list *list, const storage_ctx_t *storage, uint32_t num_prefix, const char *prefix[]);
/**
 * @brief Register a route item
 *
 * @param route 路由表对象
 * @param storage
 * @param num_prefix (ref. length of arraydup_cstring)
 * @param prefix (ref. array of arraydup_cstring)
 * @return int errno (EEXIST ENOMEM)
 */
int route_register(void *route, const storage_ctx_t *storage, uint32_t num_prefix, const char *prefix[]);
/**
 * @brief Unregister a route item by name（如果表项仍被引用，则无法注销）
 *
 * @param route 路由表对象
 * @param name 路由表项的名称。传入NULL时，注销所有表项
 * @return int errno (ENOENT EBUSY)
 */
int route_unregister(void *route, const char *name);
/**
 * @brief Get storage of the route item that matches key
 *
 * @param route 路由表对象
 * @param key
 * @param storage 返回存储上下文，并增加该表项的引用计数
 * @return int errno (ENOENT)
 */
int route_match(void *route, const char *key, const storage_ctx_t **storage);

/**
 * @brief 减少存储上下文所在表项的引用计数
 *
 * @param route
 * @param storage
 */
void route_deref(const storage_ctx_t *storage);

#endif /* __PROPD_ROUTE_H */
