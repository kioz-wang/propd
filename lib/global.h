/**
 * @file global.h
 * @author kioz.wang (never.had@outlook.com)
 * @brief
 * @version 0.1
 * @date 2025-12-04
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

#ifndef __PROPD_GLOBAL_H
#define __PROPD_GLOBAL_H

extern const char *g_at;
extern void       *g_credbook;
extern void       *g_pool;
extern void       *g_nmtx_ns;
extern void       *g_cache;
extern void       *g_route;

#define PathFmt_CtrlServer "%s/propd.%s.ctrl"
#define PathFmt_IOServer   "%s/propd.%s.io"
#define PathFmt_CtrlClient "%s/prop.%s.ctrl"
#define PathFmt_IOClient   "%s/prop.%s.io"

enum propd_errno {
    PROPD_E_SUCCESS = 0,
    RPOPD_E_OPNOTSUPP,
    PROPD_E_NOENT,
    PROPD_E_NOMEM,
    PROPD_E_NOBUFS,
    PROPD_E_IO,
    PROPD_E_MISC,
    /* compile-time error */
    PROPD_E_UNREACHABLE,
};

int propd_errno_map(void /* errno */);

#endif /* __PROPD_GLOBAL_H */
