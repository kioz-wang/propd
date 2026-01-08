/**
 * @file main.c
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

#include "builtin.h"
#include "ctrl_client.h"
#include "misc.h"
#include "storage.h"
#include <getopt.h>
#include <stdio.h>
#include <string.h>

extern const char *g_at;
static const char *g_server = "root";

static int command_ctrl(int argc, char *argv[]) {
    if (argc >= 2) {
        if (!strcmp(argv[1], "register_child")) {
            if (argc >= 4) return prop_register_child(g_server, argv[2], NULL, (const char **)&argv[3]);
        } else if (!strcmp(argv[1], "register_parent")) {
            if (argc >= 3) return prop_register_parent(g_server, argv[2]);
        } else if (!strcmp(argv[1], "unregister_child")) {
            if (argc >= 3) return prop_unregister_child(g_server, argv[2]);
        } else if (!strcmp(argv[1], "unregister_parent")) {
            if (argc >= 3) return prop_unregister_parent(g_server, argv[2]);
        } else if (!strcmp(argv[1], "dump_db_route")) {
            return prop_dump_db_route(g_server, NULL);
        } else if (!strcmp(argv[1], "dump_db_cache")) {
            return prop_dump_db_cache(g_server, NULL);
        }
    }

    fprintf(stderr, "ctrl\n");
    fprintf(stderr, "    register_child {name} {prefix} [prefix]...\n");
    fprintf(stderr, "    register_parent {name}\n");
    fprintf(stderr, "    unregister_child {name}\n");
    fprintf(stderr, "    unregister_parent {name}\n");
    fprintf(stderr, "    dump_db_route\n");
    fprintf(stderr, "    dump_db_cache\n");
    return UINT8_MAX;
}

static int command_get(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "get {key} [key]...\n");
        return UINT8_MAX;
    }

    int       ret     = 0;
    storage_t storage = {0};
    if (prop_unix_storage(&storage, g_server, true)) {
        return -1;
    }
    for (int i = 1; argv[i]; i++) {
        const char    *key   = argv[i];
        const value_t *value = NULL;
        ret                  = prop_storage_get(&storage, key, &value, NULL);
        if (ret == 0) {
            char buffer[512] = {0};
            puts(pd_value_fmt(buffer, sizeof(buffer), value, true));
            free((void *)value);
        } else {
            fprintf(stderr, "fail to get %s (%d)\n", key, ret);
        }
    }
    prop_storage_destructor(&storage);
    return ret;
}

static int command_set(int argc, char *argv[]) {
    if (argc < 3 || argc % 2 == 0) {
        fprintf(stderr, "set {key} {value} [{key} {value}]...\n");
        return UINT8_MAX;
    }

    int       ret     = 0;
    storage_t storage = {0};
    if (prop_unix_storage(&storage, g_server, true)) {
        return -1;
    }
    for (int i = 1; argv[i]; i += 2) {
        const char    *key       = argv[i];
        const char    *value_str = argv[i + 1];
        const value_t *value     = pd_value_parse(value_str);
        ret                      = prop_storage_set(&storage, key, value);
        if (ret == 0) {
            fprintf(stderr, "set %s to %s\n", key, value_str);
        } else {
            fprintf(stderr, "fail to set %s to %s (%d)\n", key, value_str, ret);
        }
        free((void *)value);
    }
    prop_storage_destructor(&storage);
    return ret;
}

static int command_del(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "del {key} [key]...\n");
        return UINT8_MAX;
    }

    int       ret     = 0;
    storage_t storage = {0};
    if (prop_unix_storage(&storage, g_server, true)) {
        return -1;
    }
    for (int i = 1; argv[i]; i++) {
        const char *key = argv[i];
        ret             = prop_storage_del(&storage, key);
        if (ret == 0) {
            fprintf(stderr, "del %s\n", key);
        } else {
            fprintf(stderr, "fail to del %s (%d)\n", key, ret);
        }
    }
    prop_storage_destructor(&storage);
    return ret;
}

int main(int argc, char *argv[]) {
    int opt;

    while ((opt = getopt(argc, argv, "t:N:h")) != -1) {
        switch (opt) {
        case 't':
            g_server = optarg;
            break;
        case 'N':
            g_at = optarg;
            break;
        case 'h':
            fprintf(stderr, "%s [-t {server}] [-N {socket root path}] ctrl|get|set|del\n", argv[0]);
            exit(0);
            break;
        default:
            fprintf(stderr, "unknown error\n");
            exit(1);
        }
    }
    argc -= optind;
    argv += optind;

    if (argc) { /* prop {command} [...] */
        pd_attach_wait("prop_attach", '.', 2);
        if (!strcmp(argv[0], "ctrl")) {
            return command_ctrl(argc, argv);
        } else if (!strcmp(argv[0], "get")) {
            return command_get(argc, argv);
        } else if (!strcmp(argv[0], "set")) {
            return command_set(argc, argv);
        } else if (!strcmp(argv[0], "del")) {
            return command_del(argc, argv);
        }
    }

    fprintf(stderr, "need subcommand\n");
    return UINT8_MAX;
}
