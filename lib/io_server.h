/**
 * @file io_server.h
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

#ifndef __PROPD_IO_SERVER_H
#define __PROPD_IO_SERVER_H

#include "timestamp.h"
#include "value.h"
#include <linux/limits.h>
#include <stdint.h>

enum io_type {
    _io_get = 0,
    _io_set,
    _io_del,
};
typedef uint8_t io_type_t;

struct io_package {
    io_type_t   type;
    timestamp_t created;
    char        key[NAME_MAX];
    value_t     value;
} __attribute__((packed));
typedef struct io_package io_package_t;

/* Server APIs */

/**
 * @brief Start IO server
 *
 * @param name server节点名
 * @param tid 返回IO server线程id，用于终止；传入NULL时，阻塞等待
 * @return int
 */
int io_start_server(const char *name, pthread_t *tid);

#endif /* __PROPD_IO_SERVER_H */
