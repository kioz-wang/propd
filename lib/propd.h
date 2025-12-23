/**
 * @file propd.h
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

#ifndef __PROPD_H
#define __PROPD_H

#include "logger/logger.h"
#include "route.h"
#include "timestamp.h"
#include <sys/queue.h>

struct propd_config {
    mlog_level_t loglevel; /* MLOG_INFO default */
    mlogger_f    logger;   /* fputs(stdout) default */

    const char *namespace; /* Unix Sockets root path. /tmp default */

    unsigned short thread_num;             /* 0 default (0 means auto)  */
    unsigned short thread_num_max_if_auto; /* 16 default */

    timestamp_t cache_interval;         /* 0 default, unit: s (0 means disable) */
    timestamp_t cache_default_duration; /* 1 default, unit: s */

    struct route_list local_route;

    const char  *name;           /* root default */
    const char **caches;         /* nothing default */
    const char **prefixes;       /* nothing default */
    uint32_t     num_prefix_max; /* 16 default */

    const char **children;
    const char **parents;
};
typedef struct propd_config propd_config_t;

void propd_config_default(propd_config_t *config);
void propd_config_parse(propd_config_t *config, int argc, char *argv[]);
int propd_config_register(propd_config_t *config, io_t io, const char *name, uint32_t num_prefix, const char *prefix[]);
int propd_run(const propd_config_t *config);

#endif /* __PROPD_H */
