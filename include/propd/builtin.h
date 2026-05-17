/**
 * @file builtin.h
 * @author kioz.wang (never.had@outlook.com)
 * @brief
 * @version 0.1
 * @date 2025-12-15
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

#ifndef __PROPD_BRIDGE_H
#define __PROPD_BRIDGE_H

#include "storage.h"

int prop_null_storage(storage_t *ctx, const char *name);
int prop_file_storage(storage_t *ctx, const char *name, const char *dir);
int prop_unix_storage(storage_t *ctx, const char *name, bool shared);
int prop_memory_storage(storage_t *ctx, const char *name, long phy, const void *layout);
int prop_tcp_storage(storage_t *ctx, const char *name, const char *ip, unsigned short port);

extern storage_parseConfig_t prop_file_parseConfig;
extern storage_parseConfig_t prop_unix_parseConfig;
extern storage_parseConfig_t prop_memory_parseConfig;
extern storage_parseConfig_t prop_tcp_parseConfig;

#endif /* __PROPD_BRIDGE_H */
