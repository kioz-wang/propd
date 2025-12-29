/**
 * @file position.h
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

#ifndef __POSITION_H
#define __POSITION_H

#include "list_search.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    lst_search_idx_decl(key, name);
    uint32_t offset; /* 以字节为单位，在内存中的偏移 */
    uint32_t length; /* 以字节为单位，占用的内存大小（regard as `integer` if `length <= 4`, else as `data`） */
    uint32_t mask;   /* 32-bit 掩码（比特 1 连续）（仅当`length <= 4`时有效） */
} pos_t;

#define POS_ITEM(_key, _name, _offset, _length, _mask)                                                                 \
    {                                                                                                                  \
        .key    = (uint32_t)_key,                                                                                      \
        .name   = (const char *)_name,                                                                                 \
        .offset = (uint32_t)_offset,                                                                                   \
        .length = (uint32_t)_length,                                                                                   \
        .mask   = (uint32_t)_mask,                                                                                     \
    }

extern int32_t pos_read(const pos_t *pos, const void *_base, void *data, uint32_t length);
extern int32_t pos_write(const pos_t *pos, void *_base, const void *data, uint32_t length);

static inline const pos_t *pos_search(const pos_t *layout, uint32_t key) { return lst_search_id(key, layout, pos_t); }
static inline const pos_t *pos_search_by_name(const pos_t *layout, const char *name) {
    return lst_search_st(name, layout, pos_t);
}

#ifdef __cplusplus
}
#endif

#endif
