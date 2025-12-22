/**
 * @file position.c
 * @author kioz.wang
 * @brief
 * @version 1.0
 * @date 2025-09-19
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

#include "position.h"
#include <errno.h>
#include <string.h>

static inline uint32_t MaskShift(uint32_t mask) { return __builtin_ffs(mask) - 1; }
static inline uint32_t MaskMax(uint32_t mask) { return mask >> MaskShift(mask); }
static inline uint32_t MaskRead(uint32_t mask, uint32_t data) { return (data & mask) >> MaskShift(mask); }
static inline uint32_t MaskWrite(uint32_t mask, uint32_t data, uint32_t value) {
    return (data & ~mask) | ((value << MaskShift(mask)) & mask);
}

static uint32_t MaskLength(uint32_t mask) {
    uint32_t v = MaskMax(mask) >> 8;
    return v ? (1 + MaskLength(v)) : 1;
}

int32_t pos_read(const pos_t *pos, const void *_base, void *data, uint32_t length) {
    const void *base = (uint8_t *)_base + pos->offset;

    if (pos->length <= sizeof(uint32_t)) {
        if (length < MaskLength(pos->mask)) return -ENOBUFS;

        uint32_t raw = 0;
        memcpy(&raw, base, pos->length);
        uint32_t value = MaskRead(pos->mask, raw);

        memset(data, 0, length);
        memcpy(data, &value, MaskLength(pos->mask));
    } else {
        if (length < pos->length) return -ENOBUFS;

        memcpy(data, base, pos->length);
        memset((uint8_t *)data + pos->length, 0, length - pos->length);
    }
    return 0;
}

int32_t pos_write(const pos_t *pos, void *_base, const void *data, uint32_t length) {
    void *base = (uint8_t *)_base + pos->offset;

    if (pos->length <= sizeof(uint32_t)) {
        if (length > sizeof(uint32_t)) return -ENOBUFS;

        uint32_t value = 0;
        memcpy(&value, data, length);
        if (value > MaskMax(pos->mask)) return -ENOBUFS;

        uint32_t raw = 0;
        memcpy(&raw, base, pos->length);
        raw = MaskWrite(pos->mask, raw, value);
        memcpy(base, &raw, pos->length);
    } else {
        if (length > pos->length) return -ENOBUFS;

        memcpy(base, data, length);
        memset((uint8_t *)base + length, 0, pos->length - length);
    }
    return 0;
}
