/**
 * @file io.h
 * @author kioz.wang (never.had@outlook.com)
 * @brief
 * @version 0.1
 * @date 2025-12-26
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

#include "storage.h"

struct io_ctx {
    void *nmtx_ns;
    void *cache;
    void *route;
};
typedef struct io_ctx io_ctx_t;

/**
 * @brief Get key on server end
 *
 * @param io
 * @param key
 * @param value
 * @param duration
 * @return int errno
 */
int io_get(const io_ctx_t *io, const char *key, const value_t **value, timestamp_t *duration);
/**
 * @brief Update key cache on server end (Only used in register_child of ctrl server)
 *
 * @param io
 * @param key
 * @param storage
 * @return int errno
 */
int io_update(const io_ctx_t *io, const char *key, const storage_t *storage);
/**
 * @brief Set key on server end
 *
 * @param io
 * @param key
 * @param value
 * @return int errno
 */
int io_set(const io_ctx_t *io, const char *key, const value_t *value);
/**
 * @brief Del key on server end
 *
 * @param io
 * @param key
 * @return int errno
 */
int io_del(const io_ctx_t *io, const char *key);

#endif /* __PROPD_IO_H */
