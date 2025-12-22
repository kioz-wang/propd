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
#include <asm-generic/errno.h>

struct io {
    void *priv;
    /**
     * @brief Get value (allocated) and duration of a key
     *
     * @param priv
     * @param key
     * @param value
     * @param duration
     * @return int
     */
    int (*get)(void *priv, const char *, const value_t **, timestamp_t *);
    /**
     * @brief Set a key with value and duraion
     *
     * @param priv
     * @param key
     * @param value
     * @return int
     */
    int (*set)(void *priv, const char *, const value_t *);
    /**
     * @brief Delete a key
     *
     * @param priv
     * @param key
     * @return int
     */
    int (*del)(void *priv, const char *);
    /**
     * @brief Close io
     *
     * @param priv
     */
    void (*deinit)(void *priv);
};
typedef struct io io_t;

/**
 * @brief
 *
 * @param io
 * @param key
 * @param value
 * @param duration （可以传入NULL）
 * @return int
 */
static inline int io_get(const io_t *io, const char *key, const value_t **value, timestamp_t *duration) {
    timestamp_t _duration;
    int         ret = io->get ? io->get(io->priv, key, value, &_duration) : EOPNOTSUPP;
    if (!ret && duration)
        *duration = _duration;
    return ret;
}
static inline int io_set(const io_t *io, const char *key, const value_t *value) {
    return io->set ? io->set(io->priv, key, value) : EOPNOTSUPP;
}
static inline int  io_del(const io_t *io, const char *key) { return io->del ? io->del(io->priv, key) : EOPNOTSUPP; }
static inline void io_deinit(const io_t *io) {
    if (io->deinit)
        io->deinit(io->priv);
}

#endif /* __PROPD_IO_H */
