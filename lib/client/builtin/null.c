/**
 * @file null.c
 * @author kioz.wang (never.had@outlook.com)
 * @brief
 * @version 0.1
 * @date 2026-01-04
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

#include "builtin.h"
#include "global.h"

#define logFmtHead "[storage::(null)] "

static int set(void *priv __attribute__((unused)), const char *key __attribute__((unused)),
               const value_t *value __attribute__((unused))) {
    return 0;
}
static int del(void *priv __attribute__((unused)), const char *key __attribute__((unused))) { return 0; }

int prop_null_storage(storage_t *ctx, const char *name) {
    if (!(ctx->name = strdup(name))) {
        logfE(logFmtHead "fail to allocate name" logFmtErrno, logArgErrno);
        return errno;
    }

    ctx->priv       = NULL;
    ctx->get        = NULL;
    ctx->set        = (typeof(ctx->set))set;
    ctx->del        = (typeof(ctx->del))del;
    ctx->destructor = NULL;
    return 0;
}
