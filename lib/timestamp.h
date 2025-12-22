/**
 * @file timestamp.h
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

#ifndef __PROPD_TIMESTAMP_H
#define __PROPD_TIMESTAMP_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

/**
 * @brief unit: nano second
 *
 */
typedef int64_t timestamp_t;

/**
 * @brief Monotonic timestamp
 *
 * @param monotonic
 * @return timestamp_t
 */
static inline timestamp_t timestamp(bool monotonic) {
    struct timespec ts;
    clock_gettime(monotonic ? CLOCK_MONOTONIC : CLOCK_REALTIME, &ts);
    return ts.tv_sec * 1000000000L + ts.tv_nsec;
}

/**
 * @brief Cast timestamp to standard timespec
 *
 * @param t
 * @return struct timespec
 */
static inline struct timespec timestamp2spec(timestamp_t t) {
    struct timespec ts = {
        .tv_sec  = t / 1000000000L,
        .tv_nsec = t % 1000000000L,
    };
    return ts;
}

/**
 * @brief Monotonic feature
 *
 * @param monotonic
 * @param ms
 * @return struct timespec
 */
static inline timestamp_t feature(bool monotonic, unsigned int ms) { return timestamp(monotonic) + ms * 1000000L; }

#define timestamp_from_ms(ms) ((timestamp_t)(ms) * 1000000L)
#define timestamp_from_s(s)   ((timestamp_t)(s) * 1000000000L)
#define timestamp_to_ms(t)    ((t) / 1000000L)
#define timestamp_to_s(t)     ((t) / 1000000000L)

#endif /* __PROPD_TIMESTAMP_H */
