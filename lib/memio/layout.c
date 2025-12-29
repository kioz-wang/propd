/**
 * @file layout.c
 * @author kioz.wang (never.had@outlook.com)
 * @brief
 * @version 0.1
 * @date 2025-12-19
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

#include "layout.h"
#include <cjson/cJSON.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef logfE
#define logfE(fmt, ...) fprintf(stderr, fmt "\n", ##__VA_ARGS__)
#endif

static void *file_read(const char *path) {
    void *buffer = NULL;
    int   fd     = -1;

    fd = open(path, O_RDONLY);
    if (fd == -1) {
        logfE("fail to open %s (%d:%s)", path, errno, strerror(errno));
        goto exit;
    }
    struct stat st;
    if (fstat(fd, &st)) {
        logfE("fail to fstat %d (%d:%s)", fd, errno, strerror(errno));
        goto exit;
    }
    buffer = malloc(st.st_size);
    if (!buffer) {
        logfE("fail to alloc %ld to read (%d:%s)", st.st_size, errno, strerror(errno));
        goto exit;
    }

    if (st.st_size != read(fd, buffer, st.st_size)) {
        logfE("fail to read %ld bytes from %d", st.st_size, fd);
        goto exit;
    }

    close(fd);
    return buffer;

exit:
    free(buffer);
    if (fd != -1) close(fd);
    return NULL;
}

static int parse_pos(const cJSON *item, pos_t *pos) {
    cJSON *name   = cJSON_GetObjectItem(item, "name");
    cJSON *offset = cJSON_GetObjectItem(item, "offset");
    cJSON *length = cJSON_GetObjectItem(item, "length");
    cJSON *mask   = cJSON_GetObjectItem(item, "mask");
    if (!name || !offset || !length) {
        logfE("invalid pos struct");
        return -1;
    }
    pos->name = strdup(cJSON_GetStringValue(name));
    if (!pos->name) {
        logfE("fail to strdup name");
        return -1;
    }
    pos->offset = strtoul(cJSON_GetStringValue(offset), NULL, 0);
    pos->length = strtoul(cJSON_GetStringValue(length), NULL, 0);
    pos->mask   = mask ? strtoul(cJSON_GetStringValue(mask), NULL, 0) : 0;
    return 0;
}

pos_t *layout_parse(const char *config) {
    void *data = file_read(config);
    if (!data) return NULL;

    cJSON *json = cJSON_Parse(data);
    if (!cJSON_IsArray(json)) {
        logfE("require array but not");
        cJSON_Delete(json);
        free(data);
        return NULL;
    }

    int    length = cJSON_GetArraySize(json);
    pos_t *layout = (pos_t *)calloc(length + 1, sizeof(pos_t));
    if (!layout) {
        logfE("fail to alloc layout contains %d pos", length);
        cJSON_Delete(json);
        free(data);
        return NULL;
    }

    layout[length].key = LST_SEARCH_ID_END;
    for (int i = 0; i < length; i++) {
        if (parse_pos(cJSON_GetArrayItem(json, i), &layout[i])) {
            logfE("fail to parse %dth pos", i);
            for (int j = 0; j < i; j++) {
                free((void *)layout[j].name);
            }
            free(layout);
            cJSON_Delete(json);
            free(data);
            return NULL;
        }
        layout[i].key = i;
    }

    cJSON_Delete(json);
    free(data);
    return layout;
}

void layout_destroy(pos_t *layout) {
    if (!layout) return;
    for (int i = 0; layout[i].key != LST_SEARCH_ID_END; i++) {
        free((void *)layout[i].name);
    }
    free(layout);
}

uint32_t layout_length(const pos_t *layout) {
    uint32_t length = 0;
    for (int i = 0; layout[i].key != LST_SEARCH_ID_END; i++) {
        const pos_t *pos = &layout[i];
        uint32_t     end = pos->offset + pos->length;
        if (end > length) length = end;
    }
    return length;
}
