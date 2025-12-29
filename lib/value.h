/**
 * @file value.h
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

#ifndef __PROPD_VALUE_H
#define __PROPD_VALUE_H

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum value_type {
    _value_undef = 0,
    _value_data,
    _value_cstring,
    _value_i32,
    _value_u32,
    _value_i64,
    _value_u64,
    _value_float,
    _value_double,
};
typedef uint8_t value_type_t;

struct value {
    value_type_t type;
    uint32_t     length;
    uint8_t      data[];
} __attribute__((packed));
typedef struct value value_t;

static inline value_t *_value_alloc(value_type_t type, uint32_t length, const void *data) {
    value_t *value = (value_t *)malloc(sizeof(value_t) + length);
    if (value) {
        value->type   = type;
        value->length = length;
        memcpy(&value->data, data, length);
    }
    return value;
}

static inline value_t *value_data(uint32_t length, const uint8_t *data) {
    return _value_alloc(_value_data, length, data);
}
static inline value_t *value_cstring(const char *cstring) {
    return _value_alloc(_value_cstring, strlen(cstring) + 1, cstring);
}
static inline value_t *value_i32(int32_t n) { return _value_alloc(_value_i32, sizeof(n), &n); }
static inline value_t *value_u32(uint32_t n) { return _value_alloc(_value_u32, sizeof(n), &n); }
static inline value_t *value_i64(int64_t n) { return _value_alloc(_value_i64, sizeof(n), &n); }
static inline value_t *value_u64(uint64_t n) { return _value_alloc(_value_u64, sizeof(n), &n); }
static inline value_t *value_float(float n) { return _value_alloc(_value_float, sizeof(n), &n); }
static inline value_t *value_double(double n) { return _value_alloc(_value_double, sizeof(n), &n); }

/**
 * @brief
 *
 * @param value
 * @return value_t* On error, return NULL and set errno
 */
static inline value_t *value_dup(const value_t *value) { return _value_alloc(value->type, value->length, value->data); }

#define _value_to(_typ, _type)                                                                                         \
    _type value_to_##_typ(const value_t *value) {                                                                      \
        _type n;                                                                                                       \
        assert(value->type == _value_##_typ);                                                                          \
        memcpy(&n, value->data, value->length);                                                                        \
        return n;                                                                                                      \
    }
static inline _value_to(i32, int32_t);
static inline _value_to(u32, uint32_t);
static inline _value_to(i64, int64_t);
static inline _value_to(u64, uint64_t);
static inline _value_to(float, float);
static inline _value_to(double, double);

/**
 * @brief Parse and allocate a value from a cstring
 *
 * @param str
 * @return value_t* On error, return NULL and set errno
 */
value_t *value_parse(const char *str);
/**
 * @brief Format a value (Only for logging, due to potential truncation)
 *
 * @param buffer
 * @param length
 * @param value
 * @param notype
 * @return const char* Always return a pointer to buffer
 */
const char *value_fmt(char *buffer, size_t length, const value_t *value, bool notype);

#endif /* __PROPD_VALUE_H */
