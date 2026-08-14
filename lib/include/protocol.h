/**
 * @file protocol.h
 * @author kioz.wang (never.had@outlook.com)
 * @brief
 * @version 0.1
 * @date 2026-08-12
 *
 * @copyright Copyright (c) 2026
 *
 */

#ifndef __PROP_PROTOCOL_H
#define __PROP_PROTOCOL_H

#include <prop/timestamp.h>
#include <stdint.h>

enum command_id {
    IO_GET = 0,
    IO_SET,
    IO_DEL,
    IO_INFO,
    CTRL_REGISTER_CHILD,
    CTRL_REGISTER_PARENT,
    CTRL_UNREGISTER_CHILD,
    CTRL_UNREGISTER_PARENT,
};

struct package_header {
    uint32_t magic; /* "prop" */
    union {
        uint32_t cmd_id; /* request */
        uint32_t result; /* response */
    };
    timestamp_t created;
    uint32_t    payload_length;
};



struct param_in_io_get {
    const char *key;
};
struct param_out_io_get {
    timestamp_t duration;
    const char *value;
};

struct param_in_io_set {
    const char *key;
    const char *value;
};

struct param_in_io_del {
    const char *key;
};

struct param_in_io_info {
    const char *key;
};
struct param_out_io_info {
    const char *info;
    const char *chain[];
};

#endif /* __PROP_PROTOCOL_H */
