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
#include "ctrl_server.h"
#include "global.h"
#include "io/bridge.h"
#include "io/memio/layout.h"
#include "io_server.h"
#include "logger/logger.h"
#include "misc.h"
#include "named_mutex.h"
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

void    *g_credbook = NULL;
void    *g_pool     = NULL;
void    *g_nmtx_ns  = NULL;
cache_t *g_cache    = NULL;
route_t *g_route    = NULL;

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
}

static void help_message(void) {
    // clang-format off
    const char *message =
        "propd [--loglevel <LOGLEVEL>] [--namespace <DIR>] [--enable-cache <INTERVAL>] [--default-duration <INTERVAL>] [--name <NAME>] [--caches <KEYS>] [--prefixes <PREFIXES>] [--children <NAMES>] [--parents <NAMES>] [-D|--daemon]"
        " [--file <DIR>,<NAME>,<PREFIXES>]"
        " [--memory <PHY>,<LAYOUT>,<NAME>,<PREFIXES>]"
        " [--tcp <IP>,<PORT>,<NAME>,<PREFIXES>]"
        " [--unix <NAME>,<PREFIXES>]\n"
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
        "  -D, --daemon                  守护进程模式（默认阻塞在前台）\n"
        "\n"
        "  --file <DIR>,<NAME>,<PREFIXES>             注册类型为file的本地IO。DIR是file IO的根目录\n"
        "  --memory <PHY>,<LAYOUT>,<NAME>,<PREFIXES>  注册类型为memory的本地IO。PHY是memory IO需要映射的内存地址，LAYOUT是描述内存布局的json文件\n"
        "  --tcp <IP>,<PORT>,<NAME>,<PREFIXES>        注册类型为tcp的本地IO。IP，PORT是tcp IO需要连接的目标\n"
        "  --unix <NAME>,<PREFIXES>                   注册类型为unix的本地IO（与通过--children注册不同的是：不需要child具有ctrl server，且不支持“立即缓存”）\n"
        "\n"
        "多个prefix之间使用逗号隔开；多个name之间使用逗号隔开；PREFIXES是支持的prefix列表\n"
        "\n";
    // clang-format on
    fputs(message, stderr);
}

void propd_config_parse(propd_config_t *config, int argc, char *argv[]) {
    const char   *shortopts  = "l:N:C:d:n:c:p:i:a:Df:m:t:u:h";
    struct option longopts[] = {
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
        {"file", required_argument, 0, 'f'},
        {"memory", required_argument, 0, 'm'},
        {"tcp", required_argument, 0, 't'},
        {"unix", required_argument, 0, 'u'},
        {0, 0, 0, 0},
        // clang-format on
    };
    int opt;
    int option_index = 0;

    while ((opt = getopt_long(argc, argv, shortopts, longopts, &option_index)) != -1) {
        switch (opt) {
        case 'l':
            config->loglevel = mlog_level_parse(optarg);
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
        case 'f':
        case 'm':
        case 't':
        case 'u': {
            size_t       length;
            const char **args = arrayparse_cstring(optarg, &length);
            if (!args) {
                fprintf(stderr, "fail to parse cstring's array seperated by comma" logFmtErrno "\n", logArgErrno);
                goto error;
            }
            const char  *name     = NULL;
            const char **prefixes = NULL;
            io_t         io;

            switch (opt) {
            case 'f': {
                if (length < 3) {
                    fprintf(stderr, "require more arguments, see: --file <DIR>,<NAME>,<PREFIXES>\n");
                    goto error;
                }
                name     = args[1];
                prefixes = &args[2];

                io = bridge_file(args[0]);
                if (bridge_is_bad(&io)) {
                    fprintf(stderr, "fail to bridge file %s\n", args[0]);
                    goto error;
                }
            } break;
            case 'm': {
                if (length < 4) {
                    fprintf(stderr, "require more arguments, see: --memory <PHY>,<LAYOUT>,<NAME>,<PREFIXES>\n");
                    goto error;
                }
                name     = args[2];
                prefixes = &args[3];

                long   phy    = strtoul(args[0], NULL, 16);
                pos_t *layout = layout_parse(args[1]);
                if (!layout) {
                    goto error;
                }

                io = bridge_memory(phy, layout);
                if (bridge_is_bad(&io)) {
                    fprintf(stderr, "fail to bridge memory %s,%s\n", args[0], args[1]);
                    goto error;
                }
            } break;
            case 't': {
                if (length < 4) {
                    fprintf(stderr, "require more arguments, see: --tcp <IP>,<PORT>,<NAME>,<PREFIXES>\n");
                    goto error;
                }
                name     = args[2];
                prefixes = &args[3];

                unsigned short port = strtoul(args[1], NULL, 0);

                io = bridge_tcp(args[0], port);
                if (bridge_is_bad(&io)) {
                    fprintf(stderr, "fail to bridge tcp %s,%s\n", args[0], args[1]);
                    goto error;
                }
            } break;
            case 'u': {
                if (length < 2) {
                    fprintf(stderr, "require more arguments, see: --unix <NAME>,<PREFIXES>\n");
                    goto error;
                }
                name     = args[0];
                prefixes = &args[1];

                io = bridge_unix(args[0]);
                if (bridge_is_bad(&io)) {
                    fprintf(stderr, "fail to bridge unix %s\n", args[0]);
                    goto error;
                }
            } break;
            }

            route_item_t *item = route_item_create(io, name, 0, prefixes);
            if (!item) {
                fprintf(stderr, "fail to create a route item named %s" logFmtErrno "\n", name, logArgErrno);
                io_deinit(&io);
                goto error;
            }
            LIST_INSERT_HEAD(&config->local_route, item, entry);
            arrayfree_cstring(args);
        } break;
        case 'h':
            help_message();
            exit(0);
            break;
        case '?':
            fprintf(stderr, "unknown option\n"); /* TODO show option */
            exit(1);
        default:
            fprintf(stderr, "unknown error\n");
            exit(1);
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

int propd_config_register(propd_config_t *config, io_t io, const char *name, uint32_t num_prefix,
                          const char *prefix[]) {
    route_item_t *item = route_item_create(io, name, num_prefix, prefix);
    if (!item) {
        fprintf(stderr, "fail to create a route item named %s" logFmtErrno "\n", name, logArgErrno);
        return errno;
    }
    LIST_INSERT_HEAD(&config->local_route, item, entry);
    return 0;
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
        g_route->list.lh_first = config->local_route.lh_first;
        if (config->local_route.lh_first) {
            config->local_route.lh_first->entry.le_prev = &g_route->list.lh_first;
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
