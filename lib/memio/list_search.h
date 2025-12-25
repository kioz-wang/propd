/**
 * @file list_search.h
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

#ifndef __LIST_SEARCH_H
#define __LIST_SEARCH_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define lst_search_idx_decl(id, st)                                                                                    \
    uint32_t    id; /* 便于索引的编号 */                                                                               \
    const char *st  /* 便于阅读的名称（可以不使用；也可用于索引） */

#ifndef LST_SEARCH_ID_END
#define LST_SEARCH_ID_END UINT32_MAX
#endif

extern const void *__lst_search(bool by_id, const void *idx, const void *lst, uint32_t itmsz);

static inline const void *__lst_search_id(uint32_t id, const void *lst, uint32_t itmsz) {
    return __lst_search(true, (const void *)&id, lst, itmsz);
}
static inline const void *__lst_search_st(const char *st, const void *lst, uint32_t itmsz) {
    return __lst_search(false, (const void *)st, lst, itmsz);
}

#define lst_search_id(id, lst, tp) (tp *)__lst_search_id(id, lst, sizeof(tp))
#define lst_search_st(st, lst, tp) (tp *)__lst_search_st(st, lst, sizeof(tp))

#ifdef __cplusplus
}
#endif

#endif
