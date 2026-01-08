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
 * - Except for priv, none of the other arguments will ever be null
 *
 * The constructor is used to populate this context. It returns 0 on success, errno otherwise.
 */
struct storage {
    const char *name; /* duplicated in constructor, release in destructor */
    void       *priv; /* allocated in constructor, release in destructor */
    int (*get)(void *priv, const char *, const value_t **, timestamp_t *);
    int (*set)(void *priv, const char *, const value_t *);
    int (*del)(void *priv, const char *);
    void (*destructor)(void *priv);
};
typedef struct storage storage_t;

/**
 * @brief
 *
 * @param storage
 * @param key
 * @param value
 * @param duration maybe null
 * @return int errno (EOPNOTSUPP ...)
 */
int prop_storage_get(const storage_t *storage, const char *key, const value_t **value, timestamp_t *duration);
/**
 * @brief
 *
 * @param storage
 * @param key
 * @param value
 * @return int errno (EOPNOTSUPP ...)
 */
int prop_storage_set(const storage_t *storage, const char *key, const value_t *value);
/**
 * @brief
 *
 * @param storage
 * @param key
 * @return int errno (EOPNOTSUPP ...)
 */
int prop_storage_del(const storage_t *storage, const char *key);
/**
 * @brief
 *
 * @param storage
 */
void prop_storage_destructor(const storage_t *storage);

/**
 * storage 的命令行解析配置，通过 propd_config_apply_parser 注册到 propd_config 中后，将自动生成 help message
 * 并参数解析。
 *
 * parse 是需要实现的 constructor 包装。第二个参数是实例名，第三个参数是通过 arrayparse_cstring 解析得到的 cstring's
 * array，对应 argName 中的每个参数。It returns 0 on success, errno otherwise.
 */
struct storage_parseConfig {
    const char *name;    /* 类型名（storage 中的是实例名） */
    const char *argName; /* `,`隔开并结尾的参数字符串。会后接`<NAME>,<PREFIXES>`，输出到 help message 中 */
    const char *note;    /* 对参数的详细说明，会输出到 help message 中 */
    int         argNum;  /* 参数数量 */
    int (*parse)(storage_t *, const char *, const char **);
    LIST_ENTRY(storage_parseConfig) entry;
};
typedef struct storage_parseConfig storage_parseConfig_t;

#endif /* __PROPD_STORAGE_H */
