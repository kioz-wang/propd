/**
 * @file main.c
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

#include "builtin.h"
#include "misc.h"
#include "propd.h"

int main(int argc, char *argv[]) {
    int            ret        = 0;
    storage_t  storage    = {0};
    const char    *prefixes[] = {"*", NULL};
    propd_config_t config;

    propd_config_default(&config);

    propd_config_apply_parser(&config, &prop_file_parseConfig);
    propd_config_apply_parser(&config, &prop_unix_parseConfig);
    propd_config_apply_parser(&config, &prop_memory_parseConfig);
    propd_config_apply_parser(&config, &prop_tcp_parseConfig);

    pd_attach_wait("propd_attach", '.', 2);
    propd_config_parse(&config, argc, argv);

    ret = prop_null_storage(&storage, "null");
    if (ret) return ret;
    ret = propd_config_register(&config, &storage, 0, prefixes);
    if (ret) return ret;

    return propd_run(&config);
}
