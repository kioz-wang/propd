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
#include "ctrl_server.h"
#include "global.h"
#include "io.h"
#include "io_server.h"
#include "logger/logger.h"
#include "misc.h"
#include "named_mutex.h"
#include "route.h"
#include "thread_pool.h"
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

const char *g_at = "/tmp";

void __attribute__((constructor)) __propd_env_parse(void) {
    const char *namespace_s = getenv("propd_namespace");
    if (namespace_s && namespace_s[0]) {
        g_at = namespace_s;
    }
}

void *g_credbook = NULL;
void *g_pool     = NULL;
void *g_nmtx_ns  = NULL;
void *g_cache    = NULL;
void *g_route    = NULL;

int propd_errno_map(void /* errno */) {
    ;
    return 0;
}

void propd_config_default(propd_config_t *config) {
    config->loglevel = MLOG_INFO;
    config->logger   = NULL;

    config->namespace = NULL;

    config->thread_num             = 0;
    config->thread_num_max_if_auto = 16;

    config->cache_interval         = 0;
    config->cache_default_duration = 1;

    LIST_INIT(&config->local_route);

    config->name           = NULL;
    config->caches         = NULL;
    config->prefixes       = NULL;
    config->num_prefix_max = 16;
    config->children       = NULL;
    config->parents        = NULL;
    config->daemon         = false;

    LIST_INIT(&config->io_parseConfigs);
}

int propd_config_register(propd_config_t *config, io_ctx_t *io_ctx, uint32_t num_prefix, const char *prefix[]) {
    route_item_t *item = route_item_create(io_ctx, num_prefix, prefix);
    if (!item) {
        fprintf(stderr, "fail to create a route item named %s" logFmtErrno "\n", io_ctx->name, logArgErrno);
        return errno;
    }
    LIST_INSERT_HEAD(&config->local_route, item, entry);
    return 0;
}

static void help_message(const propd_config_t *config) {
    io_parseConfig_t *parseConfig;
    const char       *message;

    // clang-format off
    fputs("propd [--loglevel <LOGLEVEL>] [--namespace <DIR>] [--enable-cache <INTERVAL>] [--default-duration <INTERVAL>] [--name <NAME>] [--caches <KEYS>] [--prefixes <PREFIXES>] [--children <NAMES>] [--parents <NAMES>] [-D|--daemon]\n", stderr);
    // clang-format on

    LIST_FOREACH(parseConfig, &config->io_parseConfigs, entry) {
        fprintf(stderr, " [--%s %s,<NAME>,<PREFIXES>]\n", parseConfig->name, parseConfig->argName);
    }

    // clang-format off
    message =
        "\n"
        "  --loglevel <LOGLEVEL>         指定日志等级（取值（大小写不敏感）：ERRO|WARN|INFO|VERB|DEBG；默认：INFO）\n"
        "  --namespace <DIR>             指定Unix域套接字的根路径（默认：/tmp）\n"
        "  --enable-cache <INTERVAL>     使能cache，并设定过期回收的间隔（默认：0 不使能；单位：秒）\n"
        "  --default-duration <INTERVAL> 设定默认的cache有效期（默认：1；单位：秒）\n"
        "  --name <NAME>                 指定自身的名字（默认：root）\n"
        "  --caches <KEYS>               作为子节点时，注册到父节点后需要立即缓存的key列表（默认：无）\n"
        "  --prefixes <PREFIXES>         作为子节点时，注册到父节点后支持的prefix列表（默认：*）\n"
        "  --children <NAMES>            子节点列表，主动请求子节点来注册（默认：无）\n"
        "  --parents <NAMES>             父节点列表，主动注册到父节点（默认：无）\n"
        "  -D, --daemon                  守护进程模式（默认阻塞在前台）\n";
    // clang-format on
    fputs(message, stderr);

    if (!LIST_EMPTY(&config->io_parseConfigs)) {
        fputc('\n', stderr);
    }
    LIST_FOREACH(parseConfig, &config->io_parseConfigs, entry) {
        fprintf(stderr, "  --%s %s,<NAME>,<PREFIXES>\t %s\n", parseConfig->name, parseConfig->argName,
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
    {"loglevel", required_argument, 0, 'l'},
    {"namespace", required_argument, 0, 'N'},
    {"enable-cache", required_argument, 0, 'C'},
    {"default-duration", required_argument, 0, 'd'},
    {"name", required_argument, 0, 'n'},
    {"caches", required_argument, 0, 'c'},
    {"prefixes", required_argument, 0, 'p'},
    {"children", required_argument, 0, 'i'},
    {"parents", required_argument, 0, 'a'},
    {"daemon", no_argument, 0, 'D'},
    {0, 0, 0, 0},
    // clang-format on
};

void propd_config_apply_parser(propd_config_t *config, io_parseConfig_t *parseConfig) {
    LIST_INSERT_HEAD(&config->io_parseConfigs, parseConfig, entry);
}

void propd_config_parse(propd_config_t *config, int argc, char *argv[]) {
    const char       *shortopts   = "hvN:n:D";
    struct option    *longopts    = NULL;
    int               num_longopt = 0;
    io_parseConfig_t *parseConfig;
    int               num_parseConfig = 0;

    for (int i = 0; g_longopts[i].name; i++) {
        num_longopt++;
    }
    LIST_FOREACH(parseConfig, &config->io_parseConfigs, entry) { num_parseConfig++; }
    longopts = (struct option *)calloc(num_longopt + num_parseConfig + 1, sizeof(struct option));
    if (!longopts) {
        fprintf(stderr, "fail to allocate memeory to parse" logFmtErrno "\n", logArgErrno);
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < num_longopt; i++) {
        longopts[i] = g_longopts[i];
    }
    int opt = 0;
    LIST_FOREACH(parseConfig, &config->io_parseConfigs, entry) {
        longopts[num_longopt + opt].name    = parseConfig->name;
        longopts[num_longopt + opt].has_arg = required_argument;
        longopts[num_longopt + opt].flag    = 0;
        longopts[num_longopt + opt].val     = opt;
        opt++;
    }

    int option_index = 0;
    while ((opt = getopt_long(argc, argv, shortopts, longopts, &option_index)) != -1) {
        switch (opt) {
        case 'l':
            config->loglevel = mlog_level_parse(optarg);
            break;
        case 'v':
            config->loglevel = MLOG_VERB;
            break;
        case 'N':
            config->namespace = optarg;
            break;
        case 'C':
            config->cache_interval = strtoul(optarg, NULL, 0);
            break;
        case 'd':
            config->cache_default_duration = strtoul(optarg, NULL, 0);
            break;
        case 'n':
            config->name = optarg;
            break;
        case 'c': {
            const char **args = arrayparse_cstring(optarg, NULL);
            if (!args) {
                fprintf(stderr, "fail to parse cstring's array seperated by comma" logFmtErrno "\n", logArgErrno);
                goto error;
            }
            config->caches = args;
        } break;
        case 'p': {
            const char **args = arrayparse_cstring(optarg, NULL);
            if (!args) {
                fprintf(stderr, "fail to parse cstring's array seperated by comma" logFmtErrno "\n", logArgErrno);
                goto error;
            }
            config->prefixes = args;
        } break;
        case 'i': {
            const char **args = arrayparse_cstring(optarg, NULL);
            if (!args) {
                fprintf(stderr, "fail to parse cstring's array seperated by comma" logFmtErrno "\n", logArgErrno);
                goto error;
            }
            config->children = args;
        } break;
        case 'a': {
            const char **args = arrayparse_cstring(optarg, NULL);
            if (!args) {
                fprintf(stderr, "fail to parse cstring's array seperated by comma" logFmtErrno "\n", logArgErrno);
                goto error;
            }
            config->parents = args;
        } break;
        case 'D':
            config->daemon = true;
            break;
        case 'h':
            help_message(config);
            exit(0);
            break;
        case '?':
            exit(1);
        default: {
            int i = 0;
            LIST_FOREACH(parseConfig, &config->io_parseConfigs, entry) {
                if (i == opt) break;
            }
            int          length;
            const char **args = arrayparse_cstring(optarg, &length);
            if (!args) {
                fprintf(stderr, "fail to parse cstring's array seperated by comma" logFmtErrno "\n", logArgErrno);
                goto error;
            }
            if (length < parseConfig->argNum + 2) {
                fprintf(stderr, "require more arguments, see: --%s %s,<NAME>,<PREFIXES>\n", parseConfig->name,
                        parseConfig->argName);
                goto error;
            }
            const char  *name     = args[parseConfig->argNum];
            const char **prefixes = &args[parseConfig->argNum + 1];
            io_ctx_t    *io_ctx   = parseConfig->parse(name, args);
            if (!io_ctx) {
                fprintf(stderr, "fail to create a route item named %s" logFmtErrno "\n", name, logArgErrno);
                goto error;
            }
            route_item_t *item = route_item_create(io_ctx, 0, prefixes);
            if (!item) {
                fprintf(stderr, "fail to create a route item named %s" logFmtErrno "\n", name, logArgErrno);
                io_destructor(io_ctx);
                goto error;
            }
            LIST_INSERT_HEAD(&config->local_route, item, entry);
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
    while (!LIST_EMPTY(&config->local_route)) {
        route_item_t *item = LIST_FIRST(&config->local_route);
        LIST_REMOVE(item, entry);
        route_item_destroy(item);
    }
    exit(1);
}

static void nop(int _) { (void)_; }

static int __propd_run(const propd_config_t *config, int *syncfd) {
    int ret = 0;

    g_at = config->namespace ? config->namespace : "/tmp";
    if (!ret) {
        if (access(g_at, F_OK) == -1) {
            ret = mkdir(g_at, 0755);
            if (ret) {
                logfE("fail to create namespace of Unix Sockets" logFmtRet, ret);
            }
        }
    }

    if (!ret) {
        g_pool = thread_pool_create(config->thread_num, 5, config->thread_num_max_if_auto, 0);
        if (!g_pool) {
            logfE("fail to create thread pool" logFmtErrno, logArgErrno);
            ret = -1;
        }
    }

    if (!ret) {
        g_nmtx_ns = named_mutex_create_namespace();
        if (!g_nmtx_ns) {
            logfE("fail to create namespace of named mutexes" logFmtErrno, logArgErrno);
            ret = -1;
        }
    }

    if (!ret && config->cache_interval) {
        g_cache = cache_create(timestamp_from_ms(500), timestamp_from_s(config->cache_interval),
                               timestamp_from_s(config->cache_default_duration), timestamp_from_ms(100));
        if (!g_cache) {
            logfE("fail to enable cache" logFmtErrno, logArgErrno);
            ret = -1;
        }
    }

    if (!ret) {
        g_route = route_create();
        if (!g_route) {
            logfE("fail to create route" logFmtErrno, logArgErrno);
            ret = -1;
        }
    }

    if (!ret) {
        ((route_t *)g_route)->list.lh_first = config->local_route.lh_first;
        if (config->local_route.lh_first) {
            config->local_route.lh_first->entry.le_prev = &((route_t *)g_route)->list.lh_first;
            // config->local_route.lh_first = NULL;
        }
    }

    const char *name = config->name ? config->name : "root";
    pthread_t   ctrl_tid, io_tid;
    pthread_t  *ctrl_tid_p = NULL;
    pthread_t  *io_tid_p   = NULL;

    if (!ret) {
        ret = io_start_server(name, &io_tid);
        if (ret) {
            logfE("fail to start io server" logFmtErrno, logArgErrno);
        } else ctrl_tid_p = &ctrl_tid;
    }

    if (!ret) {
        ret = ctrl_start_server(name, config->num_prefix_max, config->caches, config->prefixes, &ctrl_tid);
        if (ret) {
            logfE("fail to start ctrl server" logFmtErrno, logArgErrno);
        } else io_tid_p = &io_tid;
    }

    if (!ret && config->children) {
        for (int i = 0; config->children[i]; i++) {
            ret = ctrl_register_parent(config->children[i], name);
            if (ret) {
                logfE("fail to register <%s> to self" logFmtRet, config->children[i], ret);
                break;
            }
        }
    }
    if (!ret && config->parents) {
        for (int i = 0; config->parents[i]; i++) {
            ret = ctrl_register_parent(name, config->parents[i]);
            if (ret) {
                logfE("fail to register self to <%s>" logFmtRet, config->parents[i], ret);
                break;
            }
        }
    }

    if (syncfd && !ret) {
        int zero = 0;
        write(*syncfd, &zero, sizeof(zero));
    }

    if (!ret) {
        logfI("propd <%s> is running", name);
        signal(SIGINT, nop);
        signal(SIGTERM, nop);
        pause();
        logfI("propd <%s> is being cleaned up", name);
        if (getenv("PROPD_ATTACH")) dotwait('.', 2, 10);
    }

    if (syncfd && ret) {
        write(*syncfd, &ret, sizeof(ret));
    }

    if (ctrl_tid_p) pthread_cancel(*ctrl_tid_p);
    if (io_tid_p) pthread_cancel(*io_tid_p);
    if (g_route) {
        if (config->parents) {
            for (int i = 0; config->parents[i]; i++) {
                ctrl_unregister_child(config->parents[i], name);
            }
        }
        while (!route_unregister(g_route, NULL))
            ;
    }
    route_destroy(g_route);
    cache_destroy(g_cache);
    named_mutex_destroy_namespace(g_nmtx_ns);
    thread_pool_destroy(g_pool);

    if (syncfd && ret) {
        write(*syncfd, &ret, sizeof(ret));
    }

    return ret;
}

int propd_run(const propd_config_t *config) {
    int ret = 0;
    propd_set_logger(config->loglevel, config->logger);

    if (!config->daemon) return __propd_run(config, NULL);

    int fd[2];

    if (pipe(fd) == -1) {
        logfE("fail to create pipe" logFmtErrno, logArgErrno);
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        logfE("first fork failed" logFmtErrno, logArgErrno);
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
        logfE("second fork failed" logFmtErrno, logArgErrno);
        ret = errno;
        write(fd[1], &ret, sizeof(ret));
        write(fd[1], &ret, sizeof(ret));
        return -1;
    }

    if (pid > 0) return 0;
    return __propd_run(config, &fd[1]);
}
