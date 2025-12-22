/**
 * @file memory.c
 * @author kioz.wang (never.had@outlook.com)
 * @brief
 * @version 0.1
 * @date 2025-12-08
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

#include "cache.h"
#include "io/bridge.h"
#include "io/memio/layout.h"
#include "logger/logger.h"
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#define logFmtHead  "[bridge][memory] "
#define logFmtKey   "<%s> "
#define logFmtValue "\"%s\""

struct priv {
    void        *base;
    const pos_t *layout;
};
typedef struct priv priv_t;

static void memory_deinit(priv_t *priv) {
    munmap(priv->base, layout_length(priv->layout));
    layout_destroy((pos_t *)priv->layout);
    free(priv);
}

static int memory_get(const priv_t *priv, const char *key, const value_t **value, timestamp_t *duration) {
    const pos_t *pos = pos_search_by_name(priv->layout, key);
    if (!pos) {
        return ENOENT;
    }

    value_t *_value = (value_t *)malloc(sizeof(value_t) + pos->length);
    if (!_value) {
        return errno;
    }

    pos_read(pos, priv->base, _value->data, pos->length);
    _value->length = pos->length;
    _value->type   = pos->length > sizeof(uint32_t) ? _value_data : _value_u32;
    *value         = _value;
    *duration      = DURATION_INF;

    char buffer[256];
    logfI(logFmtHead logFmtKey "get " logFmtValue " with duration inf", key,
          value_fmt(buffer, sizeof(buffer), _value, false));
    return 0;
}

io_t bridge_memory(long phy, const pos_t *layout) {
    io_t io = BRIDGE_INITIALIZER;

    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd == -1) {
        logfE(logFmtHead "fail to open /dev/mem" logFmtErrno, logArgErrno);
        return io;
    }
    uint32_t len  = layout_length(layout);
    void    *base = mmap(0, len, PROT_READ, MAP_SHARED, fd, phy);
    close(fd);
    if (!base) {
        logfE(logFmtHead "fail to mmap(%lx,%x)" logFmtErrno, phy, len, logArgErrno);
        return io;
    }

    priv_t *priv = (priv_t *)malloc(sizeof(priv_t));
    if (!priv) {
        logfE(logFmtHead "fail to allocate priv" logFmtErrno, logArgErrno);
        return io;
    }
    priv->base   = base;
    priv->layout = layout;

    io.priv   = priv;
    io.get    = (typeof(io.get))memory_get;
    io.set    = NULL;
    io.del    = NULL;
    io.deinit = (typeof(io.deinit))memory_deinit;
    return io;
}
