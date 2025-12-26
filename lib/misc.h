/**
 * @file misc.h
 * @author kioz.wang (never.had@outlook.com)
 * @brief
 * @version 0.1
 * @date 2025-12-09
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

#ifndef __PROPD_MISC_H
#define __PROPD_MISC_H

#include <stdbool.h>
#include <sys/types.h>

void random_alnum(char *addr, size_t length);

/**
 * @brief Duplicate a cstring's array that maybe terminated with NULL
 *
 * @param array （可以传入NULL）
 * @param num 当array为NULL时无意义；否则若为0，预期array以NULL终止并以此确定num值
 * @return const char**
 */
const char **arraydup_cstring(const char **array, int num);
/**
 * @brief Release a cstring's array that terminated with NULL
 *
 * @param array （可以传入NULL）
 */
void arrayfree_cstring(const char **array);
/**
 * @brief Parse and allocate a cstring's array
 *
 * @param s 以逗号隔开的字符串列表
 * @param num 返回解析出的字符串数量（可以传入NULL）
 * @return const char**
 */
const char **arrayparse_cstring(const char *s, int *num);

/**
 * @brief Hexdump memory into buffer (use '.' to replace middle that too long to dump)
 *
 * @param buffer
 * @param length
 * @param memory
 * @param size
 * @param upper
 * @return const char*
 */
const char *hexmem(char *buffer, size_t length, void *memory, size_t size, bool upper);
/**
 * @brief Parse a hexdump cstring
 *
 * @param addr
 * @param len
 * @param str 可以前缀0x/0X
 * @return void*
 */
void *hex2mem(void *addr, size_t *len, const char *str);

bool prefix_match(const char *prefix, const char *str);

/**
 * @brief
 *
 * @param c putc
 * @param unit Sleep how long each time
 * @param n How many times
 */
void dotwait(char c, int unit, int n);

#endif /* __PROPD_MISC_H */
