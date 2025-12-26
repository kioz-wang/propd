/**
 * @file nref.c
 * @author kioz.wang (never.had@outlook.com)
 * @brief
 * @version 0.1
 * @date 2025-12-26
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

#include "nref.h"
#include <assert.h>

void nref_init(nref_t *nref) {
    nref->count = 0;
    pthread_mutex_init(&nref->mutex, NULL);
}

void nref_deinit(nref_t *nref) {
    assert(nref->count == -1);
    pthread_mutex_destroy(&nref->mutex);
}

bool nref_ref(nref_t *nref) {
    bool ret = false;
    if (nref->count == -1) return false;
    pthread_mutex_lock(&nref->mutex);
    if (nref->count != -1) {
        nref->count++;
        ret = true;
    }
    pthread_mutex_unlock(&nref->mutex);
    return ret;
}

void nref_deref(nref_t *nref) {
    assert(nref->count > 0);
    pthread_mutex_lock(&nref->mutex);
    nref->count--;
    pthread_mutex_unlock(&nref->mutex);
}

bool nref_take(nref_t *nref) {
    bool ret = false;
    if (nref->count) return false;
    pthread_mutex_lock(&nref->mutex);
    if (!nref->count) {
        nref->count--;
        ret = true;
    }
    pthread_mutex_unlock(&nref->mutex);
    return ret;
}
