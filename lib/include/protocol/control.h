/**
 * @file control.h
 * @author kioz.wang (never.had@outlook.com)
 * @brief Control protocol wire-format types (shared between client & server)
 * @version 0.1
 * @date 2025-12-03
 *
 * @copyright MIT License
 *
 *  Copyright (c) 2025 kioz.wang
 */

#ifndef __PROP_PROTOCOL_CONTROL_H
#define __PROP_PROTOCOL_CONTROL_H

#include <linux/limits.h>
#include <stdint.h>

enum ctrl_type {
    _ctrl_register_child = 0, /* child, prefix[] */
    _ctrl_register_parent,    /* parent */
    _ctrl_unregister_child,   /* child */
    _ctrl_unregister_parent,  /* parent */
    _ctrl_dump_db_route,      /* - */
    _ctrl_dump_db_cache,      /* - */
};
typedef uint8_t ctrl_type_t;

typedef struct {
    char     name[NAME_MAX];
    uint32_t num_cache_now;
    uint32_t num_prefix;
    char     cache_now_then_prefix[][NAME_MAX];
} __attribute__((packed)) ctrl_package_register_child_t;

struct ctrl_package {
    ctrl_type_t type;
    union {
        ctrl_package_register_child_t child;
        char                          name[NAME_MAX];
    };
} __attribute__((packed));
typedef struct ctrl_package ctrl_package_t;

#endif /* __PROP_PROTOCOL_CONTROL_H */
