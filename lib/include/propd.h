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

#include "infra/timestamp.h"
#include "storage.h"
#include <sys/queue.h>

struct propd_config {
    bool        daemon;    /* 是否以守护进程启动（默认为 false，前台启动） */
    const char *name;      /* default "root" */
    const char *namespace; /* Unix Sockets root path. default "/tmp" */

    struct {
        uint32_t    __level;        /* 不要更改 */
        const char *envname;        /* 根据环境变量配置日志等级，默认 ERRO（建议 "propd_loglevel"） */
        const char *envname_stderr; /* 根据环境变量配置 stderr 日志等级，并使能输出（建议 "propd_log2stderr"） */
        void (*f)(const char *);    /* 配置日志接口（默认为 NULL，不记录） */
    } logger;                       /* 日志等级可选值："ERRO", "WARN", "INFO", "VERB",
                                       "DEBG"，或数字（不限进制）；解析规则：无法解析或越界的将降为 DEBG */

    struct {
        unsigned short thread_num;  /* 线程数（默认为 0，根据CPU数自动选择） */
        unsigned short min_if_auto; /* 自动选择的线程数不能低于此（默认为 4） */
        unsigned short max_if_auto; /* 自动选择的线程数不能高于此（默认为 16） */
        unsigned short task_num;    /* 任务队列长度（默认为 0，等于线程数） */
    } thread_pool;

    struct {
        timestamp_t min_interval; /* 对于主动触发的缓存回收，若两次间隔小于此则忽略（默认 timestamp_from_ms(500)） */
        timestamp_t max_interval; /* 检查并执行缓存回收的周期（默认为 0，此时不开启缓存） */
        timestamp_t default_duration; /* 设置参数后，默认缓存多久（默认 timestamp_from_s(1)） */
        timestamp_t min_duration;     /* 不建议更改（默认 timestamp_from_ms(100)） */
    } cache;

    struct {
        const char **caches;         /* 当作为 child 被注册时，需立即缓存到父节点的参数（默认为 NULL） */
        const char **prefixes;       /* 当作为 child 被注册时，注册到父节点路由中的前缀（默认为 NULL） */
        uint32_t     num_prefix_max; /* 不建议更改（默认 16） */
    } ability;

    struct {
        const char **children; /* 启动后，作为 parent 主动注册这些节点（默认为 NULL） */
        const char **parents;  /* 启动后，作为 child 主动注册到这些节点（默认为 NULL） */
    } net;

    void *__static_route;                            /* 不要更改 */
    LIST_HEAD(, storage_parseConfig) __parseConfigs; /* 不要更改 */
    uint32_t __default_init;                         /* 不要更改 */
};
typedef struct propd_config propd_config_t;

void propd_config_default(propd_config_t *config);
int  propd_config_register(propd_config_t *config, const storage_t *storage, uint32_t num_prefix, const char *prefix[]);

void propd_config_apply_parser(propd_config_t *config, storage_parseConfig_t *parseConfig);
void propd_config_parse(propd_config_t *config, int argc, char *argv[]);

int propd_run(const propd_config_t *config);

#endif /* __PROPD_H */
