/**
 * @file propd.c
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

#include "propd.h"
#include "cache.h"
#include "ctrl.h"
#include "global.h"
#include "infra/named_mutex.h"
#include "infra/thread_pool.h"
#include "io.h"
#include "io_context.h"
#include "misc.h"
#include "prop.h"
#include "route.h"
#include "storage.h"
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAGIC_DEFAULT (0x70726f70u) /* prop */

void propd_config_default(propd_config_t *config) {
    config->daemon    = false;
    config->name      = "root";
    config->namespace = "/tmp";

    config->logger.__level        = MLOG_ERRO;
    config->logger.envname        = NULL;
    config->logger.envname_stderr = NULL;
    config->logger.f              = NULL;

    config->thread_pool.thread_num  = 0;
    config->thread_pool.min_if_auto = 4;
    config->thread_pool.max_if_auto = 16;
    config->thread_pool.task_num    = 0;

    config->cache.min_interval     = timestamp_from_ms(500);
    config->cache.max_interval     = 0;
    config->cache.default_duration = timestamp_from_s(1);
    config->cache.min_duration     = timestamp_from_ms(100);

    config->ability.caches         = NULL;
    config->ability.prefixes       = NULL;
    config->ability.num_prefix_max = 16;

    config->net.children = NULL;
    config->net.parents  = NULL;

    config->__static_route = NULL;
    LIST_INIT(&config->__parseConfigs);
    config->__default_init = MAGIC_DEFAULT;
}

int propd_config_register(propd_config_t *config, const storage_t *storage, uint32_t num_prefix, const char *prefix[]) {
    assert(config->__default_init == MAGIC_DEFAULT);

    if (!config->__static_route) {
        config->__static_route = route_list_create();
        if (!config->__static_route) return -ENOMEM;
    }
    return route_list_register(config->__static_route, storage, num_prefix, prefix);
}

static void help_message(const propd_config_t *config) {
    storage_parseConfig_t *parseConfig;
    const char            *message;

    // clang-format off
    fputs("propd [-D|--daemon] [-n|--name <NAME>] [-N|--namespace <DIR>] [-v|--verbose] [--enable-cache <INTERVAL>] [--default-duration <INTERVAL>] [--caches <KEYS>] [--prefixes <PREFIXES>] [--children <NAMES>] [--parents <NAMES>]", stderr);
    // clang-format on

    LIST_FOREACH(parseConfig, &config->__parseConfigs, entry) {
        fprintf(stderr, " [--%s %s<NAME>,<PREFIXES>]", parseConfig->name, parseConfig->argName);
    }

    // clang-format off
    message =
        "\n\n"
        "  -D, --daemon                  守护进程模式（默认阻塞在前台）\n"
        "  -n, --name <NAME>             指定自身的名字（默认：root）\n"
        "  -N, --namespace <DIR>         指定Unix域套接字的根路径（默认：/tmp）\n"

        "  -v, --verbose                 默认仅记录错误日志，可叠加使用该选项以记录更多日志\n"

        "  --enable-cache <INTERVAL>     使能cache，并设定过期回收的间隔（默认：0 不使能；单位：秒）\n"
        "  --default-duration <INTERVAL> 设定默认的cache有效期（默认：1；单位：秒）\n"

        "  --caches <KEYS>               当作为 child 被注册时，需立即缓存到父节点的参数（默认：无）\n"
        "  --prefixes <PREFIXES>         当作为 child 被注册时，注册到父节点路由中的前缀（默认：无）\n"

        "  --children <NAMES>            启动后，作为 parent 主动注册这些节点（默认：无）\n"
        "  --parents <NAMES>             启动后，作为 child 主动注册到这些节点（默认：无）\n"
        ;
    // clang-format on
    fputs(message, stderr);

    if (!LIST_EMPTY(&config->__parseConfigs)) {
        fputc('\n', stderr);
    }
    LIST_FOREACH(parseConfig, &config->__parseConfigs, entry) {
        fprintf(stderr, "  --%s %s<NAME>,<PREFIXES>\t %s\n", parseConfig->name, parseConfig->argName,
                parseConfig->note);
    }

    // clang-format off
    message =
        "\n"
        "多个prefix之间使用逗号隔开；多个name之间使用逗号隔开；PREFIXES是支持的prefix列表\n"
        "\n";
    // clang-format on
    fputs(message, stderr);
}

static const struct option g_longopts[] = {
    // clang-format off
    {"daemon", no_argument, 0, 'D'},
    {"name", required_argument, 0, 'n'},
    {"namespace", required_argument, 0, 'N'},

    {"verbose", no_argument, 0, 'v'},

    {"enable-cache", required_argument, 0, 'C'},
    {"default-duration", required_argument, 0, 'd'},

    {"caches", required_argument, 0, 'c'},
    {"prefixes", required_argument, 0, 'p'},

    {"children", required_argument, 0, 'i'},
    {"parents", required_argument, 0, 'a'},
    {0, 0, 0, 0},
    // clang-format on
};

void propd_config_apply_parser(propd_config_t *config, storage_parseConfig_t *parseConfig) {
    LIST_INSERT_HEAD(&config->__parseConfigs, parseConfig, entry);
}

void propd_config_parse(propd_config_t *config, int argc, char *argv[]) {
    assert(config->__default_init == MAGIC_DEFAULT);

    const char            *shortopts       = "hDn:N:v";
    struct option         *longopts        = NULL;
    int                    num_longopt     = 0;
    storage_parseConfig_t *parseConfig     = NULL;
    int                    num_parseConfig = 0;

    for (int i = 0; g_longopts[i].name; i++) {
        num_longopt++;
    }
    LIST_FOREACH(parseConfig, &config->__parseConfigs, entry) { num_parseConfig++; }
    longopts = (struct option *)calloc(num_longopt + num_parseConfig + 1, sizeof(struct option));
    if (!longopts) {
        fprintf(stderr, "fail to allocate memeory to parse" logFmtErrno "\n", logArgErrno);
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < num_longopt; i++) {
        longopts[i] = g_longopts[i];
    }
    int opt = 0;
    LIST_FOREACH(parseConfig, &config->__parseConfigs, entry) {
        longopts[num_longopt + opt].name    = parseConfig->name;
        longopts[num_longopt + opt].has_arg = required_argument;
        longopts[num_longopt + opt].flag    = 0;
        longopts[num_longopt + opt].val     = opt;
        opt++;
    }

    int option_index = 0;
    while ((opt = getopt_long(argc, argv, shortopts, longopts, &option_index)) != -1) {
        switch (opt) {
        case 'D':
            config->daemon = true;
            break;
        case 'n':
            config->name = optarg;
            break;
        case 'N':
            config->namespace = optarg;
            break;
        case 'v':
            config->logger.__level++;
            break;
        case 'C':
            config->cache.max_interval = timestamp_from_s(strtoul(optarg, NULL, 0));
            break;
        case 'd':
            config->cache.default_duration = timestamp_from_s(strtoul(optarg, NULL, 0));
            break;
        case 'c': {
            const char **args = arrayparse_cstring(optarg, NULL);
            if (!args) {
                fprintf(stderr, "fail to parse cstring's array seperated by comma" logFmtErrno "\n", logArgErrno);
                goto error;
            }
            config->ability.caches = args;
        } break;
        case 'p': {
            const char **args = arrayparse_cstring(optarg, NULL);
            if (!args) {
                fprintf(stderr, "fail to parse cstring's array seperated by comma" logFmtErrno "\n", logArgErrno);
                goto error;
            }
            config->ability.prefixes = args;
        } break;
        case 'i': {
            const char **args = arrayparse_cstring(optarg, NULL);
            if (!args) {
                fprintf(stderr, "fail to parse cstring's array seperated by comma" logFmtErrno "\n", logArgErrno);
                goto error;
            }
            config->net.children = args;
        } break;
        case 'a': {
            const char **args = arrayparse_cstring(optarg, NULL);
            if (!args) {
                fprintf(stderr, "fail to parse cstring's array seperated by comma" logFmtErrno "\n", logArgErrno);
                goto error;
            }
            config->net.parents = args;
        } break;
        case 'h':
            help_message(config);
            exit(0);
            break;
        case '?':
            exit(1);
        default: {
            int i = 0;
            LIST_FOREACH(parseConfig, &config->__parseConfigs, entry) {
                if (i++ == opt) break;
            }
            int          num;
            const char **args = arrayparse_cstring(optarg, &num);
            if (!args) {
                fprintf(stderr, "fail to parse cstring's array seperated by comma" logFmtErrno "\n", logArgErrno);
                goto error;
            }
            if (num < parseConfig->argNum + 2) {
                fprintf(stderr, "require more arguments, see: --%s %s<NAME>,<PREFIXES>\n", parseConfig->name,
                        parseConfig->argName);
                goto error;
            }
            const char  *name     = args[parseConfig->argNum];
            const char **prefixes = &args[parseConfig->argNum + 1];

            int       ret     = 0;
            storage_t storage = {0};
            if ((ret = parseConfig->parse(&storage, name, args))) {
                fprintf(stderr, "fail to parse a route item named %s" logFmtErrno "\n", name, logArgErrno_(ret));
                goto error;
            }
            if ((ret = route_list_register(config->__static_route, &storage, 0, prefixes))) {
                goto error;
            }
            arrayfree_cstring(args);
        } break;
        }
    }

    argc -= optind;
    argv += optind;
    if (argc) {
        fprintf(stderr, "remain arguments would be ignored\n\t");
        for (int i = 0; i < argc; i++) {
            fprintf(stderr, " %s", argv[i]);
        }
        fputc('\n', stderr);
    }

    return;

error:
    fprintf(stderr, "error occur when parse %s\n", optarg);
    route_list_destroy(config->__static_route);
    exit(1);
}

static void nop(int _) { (void)_; }

#define logFmtHead "[propd::%s] "

static int __propd_run(const propd_config_t *config, int *syncfd) {
    int      ret    = 0;
    void    *pool   = NULL;
    io_ctx_t io_ctx = {0};

    const char *name = config->name;
    if (!g_at) g_at = config->namespace;

    if (!ret) {
        if (access(g_at, F_OK) == -1) {
            ret = mkdir(g_at, 0755);
            if (ret) {
                logfE(logFmtHead "fail to create namespace of Unix Sockets" logFmtRet, name, ret);
            }
        }
    }

    if (!ret) {
        pool = thread_pool_create(config->thread_pool.thread_num, config->thread_pool.min_if_auto,
                                  config->thread_pool.max_if_auto, config->thread_pool.task_num);
        if (!pool) {
            logfE(logFmtHead "fail to create thread pool" logFmtErrno, name, logArgErrno);
            ret = -1;
        }
    }

    if (!ret) {
        io_ctx.nmtx_ns = named_mutex_create_namespace();
        if (!io_ctx.nmtx_ns) {
            logfE(logFmtHead "fail to create namespace of named mutexes" logFmtErrno, name, logArgErrno);
            ret = -1;
        }
    }

    if (!ret && config->cache.max_interval) {
        io_ctx.cache = cache_create(config->cache.min_interval, config->cache.max_interval,
                                    config->cache.default_duration, config->cache.min_duration);
        if (!io_ctx.cache) {
            logfE(logFmtHead "fail to enable cache" logFmtErrno, name, logArgErrno);
            ret = -1;
        }
    }

    if (!ret) {
        io_ctx.route = route_create(config->__static_route);
        if (!io_ctx.route) {
            logfE(logFmtHead "fail to create route" logFmtErrno, name, logArgErrno);
            ret = -1;
        }
    }

    pthread_t  ctrl_tid, io_tid;
    pthread_t *ctrl_tid_p = NULL;
    pthread_t *io_tid_p   = NULL;

    if (!ret) {
        ret = start_io_server(name, pool, NULL, &io_ctx, &io_tid);
        if (ret) {
            logfE(logFmtHead "fail to start io server" logFmtErrno, name, logArgErrno);
        } else io_tid_p = &io_tid;
    }

    if (!ret) {
        ret = start_ctrl_server(name, pool, &io_ctx, config->ability.caches, config->ability.prefixes,
                                config->ability.num_prefix_max, &ctrl_tid);
        if (ret) {
            logfE(logFmtHead "fail to start ctrl server" logFmtErrno, name, logArgErrno);
        } else ctrl_tid_p = &ctrl_tid;
    }

    if (!ret && config->net.children) {
        for (int i = 0; config->net.children[i]; i++) {
            ret = prop_register_parent(config->net.children[i], name);
            if (ret) {
                logfE(logFmtHead "fail to register <%s> to self" logFmtRet, name, config->net.children[i], ret);
                break;
            }
        }
    }
    if (!ret && config->net.parents) {
        for (int i = 0; config->net.parents[i]; i++) {
            ret = prop_register_parent(name, config->net.parents[i]);
            if (ret) {
                logfE(logFmtHead "fail to register self to <%s>" logFmtRet, name, config->net.parents[i], ret);
                break;
            }
        }
    }

    if (syncfd && !ret) {
        int zero = 0;
        write(*syncfd, &zero, sizeof(zero));
    }

    if (!ret) {
        logfI(logFmtHead "running", name);
        signal(SIGINT, nop);
        signal(SIGTERM, nop);
        pause();
        pd_attach_wait("attach_cleanup", '.', 2);
        logfI(logFmtHead "cleanup", name);
    }

    if (syncfd && ret) {
        write(*syncfd, &ret, sizeof(ret));
    }

    if (ctrl_tid_p) {
        pthread_cancel(*ctrl_tid_p);
        pthread_join(*ctrl_tid_p, NULL);
    }
    if (io_tid_p) {
        pthread_cancel(*io_tid_p);
        pthread_join(*io_tid_p, NULL);
    }
    if (io_ctx.route) {
        if (config->net.parents) {
            for (int i = 0; config->net.parents[i]; i++) {
                prop_unregister_child(config->net.parents[i], name);
            }
        }
    }
    thread_pool_destroy(pool);
    route_destroy(io_ctx.route);
    cache_destroy(io_ctx.cache);
    named_mutex_destroy_namespace(io_ctx.nmtx_ns);

    if (syncfd && ret) {
        write(*syncfd, &ret, sizeof(ret));
    }

    return ret;
}

int propd_run(const propd_config_t *config) {
    assert(config->__default_init == MAGIC_DEFAULT);

    int ret = 0;
    mlog_init(&g_logger, config->logger.__level, config->logger.f, config->logger.envname, g_logger.fmt_cfg,
              config->logger.envname_stderr, g_logger.fmt_cfg_stderr);

    if (!config->daemon) return __propd_run(config, NULL);

    const char *name = config->name;

    int fd[2];

    if (pipe(fd) == -1) {
        logfE(logFmtHead "fail to create pipe" logFmtErrno, name, logArgErrno);
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        logfE(logFmtHead "first fork failed" logFmtErrno, name, logArgErrno);
        return -1;
    }

    if (pid > 0) {
        close(fd[1]);
        read(fd[0], &ret, sizeof(ret));
        if (ret) read(fd[0], &ret, sizeof(ret));
        close(fd[0]);
        return ret;
    }

    close(fd[0]);
    setsid();

    pid = fork();
    if (pid < 0) {
        logfE(logFmtHead "second fork failed" logFmtErrno, name, logArgErrno);
        ret = errno;
        write(fd[1], &ret, sizeof(ret));
        write(fd[1], &ret, sizeof(ret));
        return -1;
    }

    if (pid > 0) return 0;
    return __propd_run(config, &fd[1]);
}
