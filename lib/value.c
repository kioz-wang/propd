/**
 * @file value.c
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

#include "value.h"
#include "misc.h"
#include <errno.h>
#include <limits.h>
#include <stdio.h>

value_t *value_parse(const char *str) {
    const char *value = str;
    const char *colon = strchr(str, ':');
    if (colon) {
        value = colon + 1;
    }

    value_type_t type = _value_cstring;
    if (colon) {
        if (prefix_match("i32:*", str)) type = _value_i32;
        else if (prefix_match("u32:*", str)) type = _value_u32;
        else if (prefix_match("i64:*", str)) type = _value_i64;
        else if (prefix_match("u64:*", str)) type = _value_u64;
        else if (prefix_match("float:*", str)) type = _value_float;
        else if (prefix_match("double:*", str)) type = _value_double;
        else if (prefix_match("data:*", str)) type = _value_data;
        else if (prefix_match("cstring:*", str)) type = _value_cstring;
        else value = str;
    }

    switch (type) {
    case _value_i32:
    case _value_i64: {
        char *endptr = NULL;
        long  number = strtol(value, &endptr, 0);
        if (endptr == value || (errno == ERANGE && (number == LONG_MAX || number == LONG_MIN))) return NULL;
        if (type == _value_i32 && (number < INT32_MIN || INT32_MAX < number)) return NULL;
        return (type == _value_i32) ? value_i32((int32_t)number) : value_i64((int64_t)number);
    } break;
    case _value_u32:
    case _value_u64: {
        char         *endptr = NULL;
        unsigned long number = strtoul(value, &endptr, 0);
        if (endptr == value || (errno == ERANGE && number == ULONG_MAX)) return NULL;
        if (type == _value_u32 && number > UINT32_MAX) return NULL;
        return (type == _value_u32) ? value_u32((uint32_t)number) : value_u64((uint64_t)number);
    } break;
    case _value_float: {
        char *endptr = NULL;
        float number = strtof(value, &endptr);
        if (endptr == value || (errno == ERANGE /* TODO check overflow and underflow */)) return NULL;
        return value_float(number);
    } break;
    case _value_double: {
        char  *endptr = NULL;
        double number = strtod(value, &endptr);
        if (endptr == value || (errno == ERANGE /* TODO check overflow and underflow */)) return NULL;
        return value_double(number);
    } break;
    case _value_data: {
        size_t   length = strlen(value) / 2;
        uint8_t *memory = malloc(length);
        if (!memory) return NULL;
        void *data = hex2mem(memory, &length, value);
        if (!data) {
            free(memory);
            return NULL;
        }
        value_t *v = value_data(length, data);
        free(memory);
        return v;
    } break;
    case _value_cstring: {
        return value_cstring(value);
    } break;
    }

    return NULL;
}

const char *value_fmt(char *buffer, size_t length, const value_t *value, bool notype) {
    switch (value->type) {
    case _value_i32:
        snprintf(buffer, length, "%s%d", notype ? "" : "i32:", value_to_i32(value));
        break;
    case _value_u32:
        snprintf(buffer, length, "%s%u", notype ? "" : "u32:", value_to_u32(value));
        break;
    case _value_i64:
        snprintf(buffer, length, "%s%ld", notype ? "" : "i64:", value_to_i64(value));
        break;
    case _value_u64:
        snprintf(buffer, length, "%s%lu", notype ? "" : "u64:", value_to_u64(value));
        break;
    case _value_float:
        snprintf(buffer, length, "%s%g", notype ? "" : "float:", value_to_float(value));
        break;
    case _value_double:
        snprintf(buffer, length, "%s%g", notype ? "" : "double:", value_to_double(value));
        break;
    case _value_data: {
        int pos = 0;
        if (!notype) {
            assert(sizeof("data:") < length);
            pos = sprintf(buffer, "%s", "data:");
        }
        hexmem(&buffer[pos], length - pos, (void *)value->data, value->length, true);
    } break;
    case _value_cstring:
        snprintf(buffer, length, "%s%s", notype ? "" : "cstring:", value->data);
        break;
    }
    return buffer;
}
