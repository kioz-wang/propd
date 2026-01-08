/**
 * @file ctrl_server.h
 * @author kioz.wang (never.had@outlook.com)
 * @brief
 * @version 0.1
 * @date 2025-12-03
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

#ifndef __PROPD_CTRL_SERVER_H
#define __PROPD_CTRL_SERVER_H

#include "io.h"
#include <linux/limits.h>
#include <pthread.h>

enum ctrl_type {
    _ctrl_register_child = 0, /* child, prefix[] */
    _ctrl_register_parent,    /* parent */
    _ctrl_unregister_child,   /* child */
    _ctrl_unregister_parent,  /* parent */
    _ctrl_dump_db_route,      /* - */
    _ctrl_dump_db_cache,      /* - */
};
typedef uint8_t ctrl_type_t;

typedef struct {
    char     name[NAME_MAX];
    uint32_t num_cache_now;
    uint32_t num_prefix;
    char     cache_now_then_prefix[][NAME_MAX];
} __attribute__((packed)) ctrl_package_register_child_t;

struct ctrl_package {
    ctrl_type_t type;
    union {
        ctrl_package_register_child_t child;
        char                          name[NAME_MAX];
    };
} __attribute__((packed));
typedef struct ctrl_package ctrl_package_t;

/* Server APIs */

/**
 * @brief Start ctrl server
 *
 * @param name server节点名
 * @param thread_pool
 * @param io_ctx
 * @param cache_now
 * @param prefix
 * @param num_prefix_max
 * @param tid 返回ctrl server线程id，用于终止；传入NULL时，阻塞等待
 * @return int errno
 */
int start_ctrl_server(const char *name, void *thread_pool, const io_ctx_t *io_ctx, const char *cache_now[],
                      const char *prefix[], uint32_t num_prefix_max, pthread_t *tid);

#endif /* __PROPD_CTRL_SERVER_H */
