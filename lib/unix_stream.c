/**
 * @file unix_stream.c
 * @author kioz.wang (never.had@outlook.com)
 * @brief
 * @version 0.1
 * @date 2026-02-11
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

#include "unix_stream.h"
#include "global.h"
#include "misc.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

int us_open_at(us_t *us, const char *at, const char *target) {
    int ret = 0;

    us->__target    = strdup(target);
    us->__fd        = -1;
    us->flags_read  = 0;
    us->flags_write = MSG_NOSIGNAL;
    us->__step      = 32;

    if (!us->__target) return -1;

    int connfd = socket(AF_LOCAL, SOCK_STREAM, 0);
    if (connfd == -1) {
        logfE("[unix --%s] fail to get socket" logFmtErrno, us->__target, logArgErrno);
        return -1;
    }

    struct sockaddr_un cliaddr = {0};
    cliaddr.sun_family         = AF_LOCAL;
    cliaddr.sun_path[0]        = '\0';
    random_alnum(&cliaddr.sun_path[1], sizeof(cliaddr.sun_path) - 2);
    cliaddr.sun_path[sizeof(cliaddr.sun_path) - 1] = 'X';

    ret = bind(connfd, (const struct sockaddr *)&cliaddr, sizeof(cliaddr));
    if (ret) {
        logfE("[unix --%s] fail at bind" logFmtErrno, us->__target, logArgErrno);
        close(connfd);
        return -1;
    }

    struct sockaddr_un servaddr = {0};
    servaddr.sun_family         = AF_LOCAL;
    snprintf(servaddr.sun_path, sizeof(servaddr.sun_path), PathFmt_IOServer, at, target);

    ret = connect(connfd, (const struct sockaddr *)&servaddr, sizeof(servaddr));
    if (ret) {
        logfE("[unix --%s] fail to connect" logFmtErrno, us->__target, logArgErrno);
        close(connfd);
        return -1;
    }

    us->__fd = connfd;
    logfI("[unix --%s] connect as %d", us->__target, us->__fd);
    return 0;
}

void us_close(us_t *us) {
    shutdown(us->__fd, SHUT_WR);
    close(us->__fd);
    logfI("[unix --%s] disconnect %d", us->__target, us->__fd);
    free(us->__target);
    us->__target = NULL;
    us->__fd     = -1;
}

void us_discard_remain(const us_t *us) {
    ssize_t n = 0, count = 0;
    char    discard[16];
    int     fl = fcntl(us->__fd, F_GETFL);
    fcntl(us->__fd, F_SETFL, fl | O_NONBLOCK);
    for (;;) {
        n = recv(us->__fd, discard, sizeof(discard), us->flags_read);
        if (n == -1) {
            logfW("[unix <-%s] %ld-byte discarded, but fail then" logFmtErrno, us->__target, count, logArgErrno);
            break;
        }
        count += n;
        if (n != sizeof(discard)) {
            logfD("[unix <-%s] %ld-byte discarded", us->__target, count);
            break;
        }
    }
    fcntl(us->__fd, F_SETFL, fl);
}

int us_read(const us_t *us, void *ptr, size_t __n) {
    ssize_t n = recv(us->__fd, ptr, __n, us->flags_read);
    if (n == -1) {
        logfW("[unix <-%s] fail to read %ld-byte" logFmtErrno, us->__target, __n, logArgErrno);
        return -1;
    }
    if ((size_t)n != __n) {
        logfE("[unix <-%s] read expect %ld-byte but %ld" logFmtErrno, us->__target, __n, n, logArgErrno);
        errno = EIO;
        return -1;
    }
    if (__n != 1) logfD("[unix <-%s] read %ld-byte", us->__target, __n);
    return 0;
}

int us_write(const us_t *us, const void *ptr, size_t __n) {
    ssize_t n = send(us->__fd, ptr, __n, us->flags_write);
    if (n == -1) {
        logfW("[unix ->%s] fail to write %ld-byte" logFmtErrno, us->__target, __n, logArgErrno);
        return -1;
    }
    if ((size_t)n != __n) {
        logfE("[unix ->%s] write expect %ld-byte but %ld" logFmtErrno, us->__target, __n, n, logArgErrno);
        errno = EIO;
        return -1;
    }
    if (__n != 1) logfD("[unix ->%s] write %ld-byte", us->__target, __n);
    return 0;
}

void *us_read_auto(const us_t *us, size_t head_length) {
    void *head = alloca(head_length);
    if (us_read(us, head, head_length)) {
        logfE("[unix <-%s] fail to read head", us->__target);
        return NULL;
    }

    uint32_t data_length = 0;
    memcpy(&data_length, &head[head_length - sizeof(uint32_t)], sizeof(uint32_t));
    void *data = malloc(head_length + data_length);
    if (!data) return NULL;

    if (us_read(us, &data[head_length], data_length)) {
        logfE("[unix <-%s] fail to read data", us->__target);
        free(data);
        return NULL;
    }

    memcpy(data, head, head_length);
    logfD("[unix <-%s] read auto with %d-byte data", us->__target, data_length);
    return data;
}

int us_write_auto(const us_t *us, size_t head_length, const void *ptr) {
    uint32_t data_length = 0;
    memcpy(&data_length, &ptr[head_length - sizeof(uint32_t)], sizeof(uint32_t));

    if (us_write(us, ptr, head_length + data_length)) {
        logfE("[unix ->%s] fail to write auto with %d-byte data", us->__target, data_length);
        return -1;
    }

    logfD("[unix ->%s] write auto with %d-byte data", us->__target, data_length);
    return 0;
}

char *us_read_cstring(const us_t *us) {
    int   count   = 0;
    char *cstring = NULL;

    for (;;) {
        cstring = realloc(cstring, us->__step * (count + 1));
        if (!cstring) return NULL;
        char *p = &cstring[us->__step * count];

        for (size_t i = 0; i < us->__step; i++) {
            if (us_read(us, p, 1)) {
                free(cstring);
                return NULL;
            }
            if (*p++ == '\0') {
                logfD("[unix <-%s] read a cstring(%ld)", us->__target, us->__step * count + i);
                return cstring;
            }
        }
    }
}

int us_write_cstring(const us_t *us, const char *ptr) {
    const char *p = ptr;
    for (;;) {
        if (us_write(us, p, 1)) return -1;
        if (*p++ == '\0') {
            logfD("[unix ->%s] write a cstring(%ld)", us->__target, p - ptr);
            return 0;
        }
    }
}

char **us_read_cstrings(const us_t *us) {
    uint32_t num = 0;
    if (us_read(us, &num, sizeof(num))) {
        logfE("[unix <-%s] fail to read cstrings'num", us->__target);
        return NULL;
    }

    char **cstrings = calloc(num + 1, sizeof(char *));
    if (!cstrings) {
        return NULL;
    }

    for (uint32_t i = 0; i < num; i++) {
        cstrings[i] = us_read_cstring(us);
        if (!cstrings[i]) {
            logfE("[unix <-%s] read cstrings[%d] but fail", us->__target, i);
            arrayfree_cstring((const char **)cstrings);
            return NULL;
        }
    }

    logfD("[unix <-%s] read cstrings(%d)", us->__target, num);
    return cstrings;
}

int us_write_cstrings(const us_t *us, const char **ptr, int num) {
    if (!num)
        while (ptr[num])
            num++;

    if (us_write(us, &num, sizeof(num))) {
        logfE("[unix ->%s] fail to write cstrings'num", us->__target);
        return -1;
    }

    for (int i = 0; i < num; i++) {
        if (us_write_cstring(us, ptr[i])) {
            logfE("[unix ->%s] write cstrings[%d] but fail", us->__target, i);
            return -1;
        }
    }

    logfD("[unix ->%s] write cstrings(%d)", us->__target, num);
    return 0;
}
