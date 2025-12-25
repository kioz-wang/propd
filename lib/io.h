/**
 * @file io.h
 * @author kioz.wang (never.had@outlook.com)
 * @brief
 * @version 0.1
 * @date 2025-12-15
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

#ifndef __PROPD_IO_H
#define __PROPD_IO_H

#include "timestamp.h"
#include "value.h"
#include <sys/queue.h>

/**
 * Requirements for IO functions:
 * - No persistent blocking (timeout needs to be implemented)
 * - Returns 0 on success, -1 otherwise (errors are passed through logs and errno)
 * - Need to consider concurrency when different keys
 */
struct io_ctx {
    const char *name; /* duplicated in constructor, release in destructor */
    void       *priv; /* allocated in constructor, release in destructor */
    int (*get)(void *priv, const char * /* not null */, const value_t ** /* not null */, timestamp_t * /* not null */);
    int (*set)(void *priv, const char * /* not null */, const value_t * /* not null */);
    int (*del)(void *priv, const char * /* not null */);
    void (*destructor)(void *priv);
};
typedef struct io_ctx io_ctx_t;

int  io_get(const io_ctx_t *io, const char *key, const value_t **value, timestamp_t *duration);
int  io_set(const io_ctx_t *io, const char *key, const value_t *value);
int  io_del(const io_ctx_t *io, const char *key);
void io_destructor(io_ctx_t *io);

struct io_parseConfig {
    const char *name;
    const char *argName;
    const char *note;
    int         argNum;
    io_ctx_t *(*parse)(const char * /* not null */, const char ** /* not null */);
    LIST_ENTRY(io_parseConfig) entry;
};
typedef struct io_parseConfig io_parseConfig_t;

#endif /* __PROPD_IO_H */
