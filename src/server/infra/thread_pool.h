/**
 * @file thread_pool.h
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

#ifndef __THREAD_POOL_H
#define __THREAD_POOL_H

#include <stdbool.h>

/**
 * @brief Allocate and initialize a thread-pool
 *
 * @param thread_num 线程数。传入0时，根据CPU数自动选择
 * @param min_if_auto 自动选择的线程数不能低于此
 * @param max_if_auto 自动选择的线程数不能高于此
 * @param task_num 任务队列长度。传入0时，等于线程数
 * @return void* 线程池对象（On error, return NULL and set errno）
 */
void *thread_pool_create(unsigned short thread_num, unsigned short min_if_auto, unsigned short max_if_auto,
                         unsigned short task_num);
/**
 * @brief Release a thread-pool, cancel all threads
 *
 * @param tpool 线程池对象
 */
void thread_pool_destroy(void *tpool);
/**
 * @brief Submit a task
 *
 * @param tpool 线程池对象
 * @param routine
 * @param arg
 * @param sync
 * @return int 当sync为true时，同步等待routine执行完毕并返回其返回值；否则始终返回0
 */
int thread_pool_submit(void *tpool, int (*routine)(void *), void *arg, bool sync);

#endif /* __THREAD_POOL_H */
