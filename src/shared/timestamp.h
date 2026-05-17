/**
 * @file timestamp.h
 * @author kioz.wang (never.had@outlook.com)
 * @brief Monotonic timestamp utilities (shared between client & server)
 * @version 0.1
 * @date 2025-12-03
 *
 * @copyright MIT License
 *
 *  Copyright (c) 2025 kioz.wang
 */

#ifndef __TIMESTAMP_H
#define __TIMESTAMP_H

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

#define DURATION_INF (INT64_MAX)

#include <stdio.h>
static inline const char *duration_fmt(char *buffer, size_t length, timestamp_t duration) {
    if (DURATION_INF == duration) snprintf(buffer, length, "inf");
    else snprintf(buffer, length, "%ldms", timestamp_to_ms(duration));
    return buffer;
}

#endif /* __TIMESTAMP_H */
