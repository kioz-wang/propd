/**
 * @file unix_stream.h
 * @author kioz.wang (never.had@outlook.com)
 * @brief
 * @version 0.1
 * @date 2026-02-11
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

#ifndef __PROPD_UNIX_STREAM_H
#define __PROPD_UNIX_STREAM_H

#include <stddef.h>

struct unix_stream {
    char  *__target;
    int    __fd;
    int    flags_read;
    int    flags_write;
    size_t __step;
};
typedef struct unix_stream us_t;

/**
 * @brief 打开unix域套接字流
 *
 * @param us
 * @param at
 * @param target
 * @return int 成功时返回0
 */
int us_open_at(us_t *us, const char *at, const char *target);
/**
 * @brief 关闭unix域套接字流
 *
 * @param us
 */
void us_close(us_t *us);
/**
 * @brief 丢弃unix域套接字流中未被读出的数据
 *
 * @param us
 */
void us_discard_remain(const us_t *us);

/**
 * @brief 读出固定长度的数据
 *
 * @param us
 * @param ptr
 * @param __n
 * @return int 成功时返回0
 */
int us_read(const us_t *us, void *ptr, size_t __n);
/**
 * @brief 写入固定长度的数据
 *
 * @param us
 * @param ptr
 * @param __n
 * @return int 成功时返回0
 */
int us_write(const us_t *us, const void *ptr, size_t __n);
/**
 * @brief 先读出固定长度的头，从头的尾部取出uint32作为变长部分的长度，再读出余下数据
 *
 * @param us
 * @param head_length
 * @return void*
 */
void *us_read_auto(const us_t *us, size_t head_length);
/**
 * @brief 从头的尾部取出uint32作为变长部分的长度，写入头和变长部分的数据
 *
 * @param us
 * @param head_length
 * @param ptr
 * @return int 成功时返回0
 */
int us_write_auto(const us_t *us, size_t head_length, const void *ptr);
/**
 * @brief 读出cstring
 *
 * @param us
 * @return char*
 */
char *us_read_cstring(const us_t *us);
/**
 * @brief 写入cstring
 *
 * @param us
 * @param ptr
 * @return int 成功时返回0
 */
int us_write_cstring(const us_t *us, const char *ptr);
/**
 * @brief 读出cstring-array。先读到数量，再按此读出所有cstring
 *
 * @param us
 * @return char**
 */
char **us_read_cstrings(const us_t *us);
/**
 * @brief 写入cstring-array。先写入数量，再写入所有cstring
 *
 * @param us
 * @param ptr
 * @param num 非0时，仅写入指定数量的cstring
 * @return int 成功时返回0
 */
int us_write_cstrings(const us_t *us, const char **ptr, int num);

#endif /* __PROPD_UNIX_STREAM_H */
