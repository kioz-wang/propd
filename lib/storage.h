/**
 * @file storage.h
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

#ifndef __PROPD_STORAGE_H
#define __PROPD_STORAGE_H

#include "infra/timestamp.h"
#include "value.h"
#include <sys/queue.h>

/**
 * Requirements for IO functions:
 * - No persistent blocking (timeout needs to be implemented)
 * - Returns 0 on success, errno otherwise
 * - Need to consider concurrency when different keys
 *
 * The constructor is used to populate this context. It returns non-zero and sets errno on failure.
 */
struct storage_ctx {
    const char *name; /* duplicated in constructor, release in destructor */
    void       *priv; /* allocated in constructor, release in destructor */
    int (*get)(void *priv, const char * /* not null */, const value_t ** /* not null */, timestamp_t * /* not null */);
    int (*set)(void *priv, const char * /* not null */, const value_t * /* not null */);
    int (*del)(void *priv, const char * /* not null */);
    void (*destructor)(void *priv);
};
typedef struct storage_ctx storage_ctx_t;

/**
 * @brief
 *
 * @param storage
 * @param key
 * @param value
 * @param duration maybe NULL
 * @return int errno
 */
int storage_get(const storage_ctx_t *storage, const char *key, const value_t **value, timestamp_t *duration);
/**
 * @brief
 *
 * @param storage
 * @param key
 * @param value
 * @return int errno
 */
int storage_set(const storage_ctx_t *storage, const char *key, const value_t *value);
/**
 * @brief
 *
 * @param storage
 * @param key
 * @return int errno
 */
int storage_del(const storage_ctx_t *storage, const char *key);
/**
 * @brief
 *
 * @param storage
 */
void storage_destructor(const storage_ctx_t *storage);

struct storage_parseConfig {
    const char *name;
    const char *argName;
    const char *note;
    int         argNum;
    int (*parse)(storage_ctx_t *, const char * /* not null */, const char ** /* not null */);
    LIST_ENTRY(storage_parseConfig) entry;
};
typedef struct storage_parseConfig storage_parseConfig_t;

#endif /* __PROPD_STORAGE_H */
