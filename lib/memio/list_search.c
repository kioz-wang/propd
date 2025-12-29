/**
 * @file list_search.c
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

#include "list_search.h"
#include <string.h>

typedef struct {
    lst_search_idx_decl(id, st);
} index_t;

static inline const void *lst_idx_of(const void *lst, uint32_t itmsz, uint32_t idx) {
    return (uint8_t *)lst + itmsz * idx;
}

const void *__lst_search(bool by_id, const void *idx, const void *lst, uint32_t itmsz) {
    uint32_t    id = 0;
    const char *st = NULL;

    if (by_id) id = *(uint32_t *)idx;
    else st = (const char *)idx;

    for (int32_t i = 0;; i++) {
        const index_t *idxp = (const index_t *)lst_idx_of(lst, itmsz, i);
        if (idxp->id == LST_SEARCH_ID_END) break;

        if (by_id) {
            if (idxp->id == id) return idxp;
        } else {
            if (!strcmp(idxp->st, st)) return idxp;
        }
    }

    return NULL;
}
