/**
 * @file io_package.h
 * @author kioz.wang (never.had@outlook.com)
 * @brief IO protocol wire-format types (shared between client & server)
 * @version 0.1
 * @date 2025-12-03
 *
 * @copyright MIT License
 *
 *  Copyright (c) 2025 kioz.wang
 */

#ifndef __PROPD_IO_PACKAGE_H
#define __PROPD_IO_PACKAGE_H

#include "shared/timestamp.h"
#include "value.h"
#include <linux/limits.h>
#include <stdint.h>

enum io_type {
    _io_get = 0,
    _io_set,
    _io_del,
    _io_info,
};
typedef uint8_t io_type_t;

struct io_package {
    io_type_t   type;
    timestamp_t created;
    char        key[NAME_MAX];
    value_t     value;
} __attribute__((packed));
typedef struct io_package io_package_t;

#endif /* __PROPD_IO_PACKAGE_H */
