/**
 * @file misc.c
 * @author kioz.wang (never.had@outlook.com)
 * @brief
 * @version 0.1
 * @date 2025-12-09
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

#define _GNU_SOURCE
#include "misc.h"
#include "infra/timestamp.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>
#include <unistd.h>

void random_alnum(char *addr, size_t length) {
    unsigned int seed;
    if (getrandom(&seed, sizeof(seed), GRND_NONBLOCK) != sizeof(seed)) {
        seed = (unsigned int)timestamp(false) ^ getpid() ^ gettid();
    }
    for (size_t i = 0; i < length; i++) {
        int  ci = rand_r(&seed) % (26 * 2 + 10);
        char c;
        if (ci >= 26 * 2) c = '0' + ci - 26 * 2;
        else if (ci >= 26) c = 'a' + ci - 26;
        else c = 'A' + ci;
        addr[i] = c;
    }
}

const char **arraydup_cstring(const char **array, int num) {
    if (array) {
        if (!num)
            while (array[num])
                num++;
    } else {
        num = 0;
    }
    const char **dup = (const char **)calloc(num + 1, sizeof(char *));
    if (!dup) return NULL;
    for (int i = 0; i < num; i++) {
        dup[i] = strdup(array[i]);
        if (!dup[i]) {
            arrayfree_cstring(dup);
            return NULL;
        }
    }
    return dup;
}

void arrayfree_cstring(const char **array) {
    if (array) {
        for (int i = 0; array[i]; i++) {
            free((void *)array[i]);
        }
        free(array);
    }
}

const char **arrayparse_cstring(const char *s, int *_num) {
    char *_s = strdup(s);
    if (!_s) return NULL;

    int   num = 0;
    char *w1  = _s;
    char *w2  = _s;

    while (*w2) {
        if (*w2 == ',') {
            num++;
            w1 = w2 + 1;
        }
        w2++;
    }
    if (w2 != _s) num++;

    const char **array = (const char **)calloc(num, sizeof(char *));
    if (!array) {
        free(_s);
        return NULL;
    }

    int i = 0;
    w1    = _s;
    w2    = _s;
    while (*w2) {
        if (*w2 == ',') {
            *w2        = '\0';
            array[i++] = w1;
            w1         = w2 + 1;
        }
        w2++;
    }
    if (w2 != _s) array[i] = w1;

    const char **dup = arraydup_cstring(array, num);
    free(array);
    free(_s);
    if (dup && _num) *_num = num;
    return dup;
}

const char *arrayfmt_cstring(char *buffer, size_t length, const char **array) {
    assert(array);
    assert(length > 3);
    int pos = 0;

    for (int i = 0; array[i]; i++) {
        int n = snprintf(&buffer[pos], length - pos, "%s,", array[i]);
        if (n < 0 || n >= (int)length - pos) {
            memset(&buffer[length - 1 - 3], '.', 3);
            return buffer;
        }
        pos += n;
    }
    buffer[pos - 1] = '\0';
    return buffer;
}

static inline uint8_t dec2hexchar(uint8_t dec, bool upper) {
    return dec >= 10 ? (dec - 10 + (upper ? 'A' : 'a')) : (dec + '0');
}

static inline uint8_t hexchar2dec(uint8_t hexchar) {
    if (hexchar >= '0' && hexchar <= '9') return hexchar - '0';
    else if (hexchar >= 'A' && hexchar <= 'F') return 10 + (hexchar - 'A');
    else /* if (hexchar >= 'a' && hexchar <= 'f') */ return 10 + (hexchar - 'a');
}

const char *hexmem(char *buffer, size_t b, void *memory, size_t m, bool upper) {
    assert(buffer);
    assert(b);

    if (!memory || !m) {
        buffer[0] = '\0';
        return buffer;
    }

    --b;
    char *ptr = buffer;

    size_t c /* capacity */ = b / 2;

    if (c >= m) {
        for (size_t i = 0; i < m; i++) {
            *ptr++ = dec2hexchar(((uint8_t *)memory)[i] >> 4, upper);
            *ptr++ = dec2hexchar(((uint8_t *)memory)[i] & 0xf, upper);
        }
        *ptr = '\0';
        return buffer;
    }

    if (c < 3) {
        memset(buffer, '.', b);
        buffer[b] = '\0';
        return buffer;
    }

    /* m > c >= 3 */
    size_t d /* delta */ = m - c + 1;
    size_t left          = (m - d) / 2;
    size_t right         = (m + d) / 2;
    for (size_t i = 0; i < left; i++) {
        *ptr++ = dec2hexchar(((uint8_t *)memory)[i] >> 4, upper);
        *ptr++ = dec2hexchar(((uint8_t *)memory)[i] & 0xf, upper);
    }
    *ptr++ = '.';
    *ptr++ = '.';
    for (size_t i = right; i < m; i++) {
        *ptr++ = dec2hexchar(((uint8_t *)memory)[i] >> 4, upper);
        *ptr++ = dec2hexchar(((uint8_t *)memory)[i] & 0xf, upper);
    }
    *ptr = '\0';
    return buffer;
}

static inline bool ishexchar(uint8_t hexchar) {
    return (hexchar >= '0' && hexchar <= '9') || (hexchar >= 'A' && hexchar <= 'F') ||
           (hexchar >= 'a' && hexchar <= 'f');
}

void *hex2mem(void *addr, size_t *len, const char *str) {
    size_t cur = 0;
    size_t i   = 0;
    if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) cur = 2;

    for (i = 0; ishexchar(str[cur]); i++) {
        if (i == *len) return NULL;
        ((uint8_t *)addr)[i] = hexchar2dec(str[cur++]) << 4;
        if (!ishexchar(str[cur])) return NULL;
        ((uint8_t *)addr)[i] |= hexchar2dec(str[cur++]);
    }
    *len = i;
    return addr;
}

bool prefix_match(const char *prefix, const char *str) {
    const char *p1 = prefix;
    const char *p2 = str;
    char        c1, c2;
    for (;;) {
        c1 = *p1++;
        c2 = *p2++;
        if (c1 == c2 && c1 == '\0') return true;
        else if (c1 != c2) break;
    }
    return c1 == '*';
}

void pd_attach_wait(const char *envname __attribute__((unused)), char c __attribute__((unused)),
                    int unit __attribute__((unused))) {
#ifndef NDEBUG
    const char *n_str = getenv(envname ? envname : "ATTACH_WAIT");
    if (!n_str) return;

    int n = strtoul(n_str, NULL, 0);
    if (!n) return;

    assert(unit);
    int b = 0;
    fputc('\n', stderr);
    do {
        b = fprintf(stderr, "%d", n);
        sleep(unit);
        for (int i = 0; i < b; i++)
            fputc('\b', stderr);
        fputc(c, stderr);
    } while (--n);
    fputc('\n', stderr);
#endif
}

#ifdef __TEST_MISC

#include <ctype.h>
static void TEST_random_alnum(void) {
    fprintf(stderr, "\n %s\n\n", __func__);
    char buffer[64 + 1] = {0};
    random_alnum(buffer, sizeof(buffer) - 1);
    fprintf(stderr, "%s\n", buffer);
    for (size_t i = 0; i < sizeof(buffer) - 1; i++)
        assert(isalnum(buffer[i]));
}

static void TEST_cstring_array(void) {
    fprintf(stderr, "\n %s\n\n", __func__);
    char         buffer0[64];
    char         buffer1[sizeof(buffer0)];
    const char  *cstring[] = {"hello", "world", "", "abc,def", "", NULL};
    const char **dup0      = arraydup_cstring(cstring, sizeof(cstring) / sizeof(cstring[0]) - 1);
    const char **dup1      = arraydup_cstring(cstring, 0);
    const char  *formated  = arrayfmt_cstring(buffer0, sizeof(buffer0), dup0);
    arrayfmt_cstring(buffer1, sizeof(buffer1), dup1);
    fprintf(stderr, "%s\n", formated);
    assert(!strncmp(buffer0, buffer1, sizeof(buffer0)));
    arrayfree_cstring(dup0);
    arrayfree_cstring(dup1);
    int          num    = 0;
    const char **parsed = arrayparse_cstring(buffer0, &num);
    assert(num == 6);
    arrayfmt_cstring(buffer1, sizeof(buffer1), parsed);
    assert(!strncmp(buffer0, buffer1, sizeof(buffer0)));
    arrayfmt_cstring(buffer1, 10, parsed);
    assert(!strncmp("hello,...", buffer1, sizeof(buffer0)));
    arrayfree_cstring(parsed);
}

static void TEST_hexmem(void) {
    fprintf(stderr, "\n %s\n\n", __func__);
    char        buffer0[16];
    char        buffer1[sizeof(buffer0)];
    const char *hex_str0 = "12345678901234567890abff";
    const char *hex_str1 = "12345678901234567890abcdefaabbccddeeff";

    assert(!hex2mem(buffer0, &(size_t){sizeof(buffer0)}, hex_str1));
    size_t hex0_len = sizeof(buffer0);
    hex2mem(buffer0, &hex0_len, hex_str0);
    assert(12 == hex0_len);

    assert(!strcmp("", hexmem(buffer1, sizeof(buffer1), NULL, 0, false)));
    random_alnum(buffer1, sizeof(buffer1));
    assert(!strcmp("1234567890", hexmem(buffer1, sizeof(buffer1), buffer0, 5, false)));
    random_alnum(buffer1, sizeof(buffer1));
    assert(!strcmp("12345678901234", hexmem(buffer1, sizeof(buffer1), buffer0, 7, false)));
    random_alnum(buffer1, sizeof(buffer1));
    assert(!strcmp("123456..123456", hexmem(buffer1, sizeof(buffer1), buffer0, 8, false)));
    random_alnum(buffer1, sizeof(buffer1));
    assert(!strcmp("123456..90abff", hexmem(buffer1, sizeof(buffer1), buffer0, hex0_len, false)));
    random_alnum(buffer1, sizeof(buffer1));
    assert(!strcmp(".....", hexmem(buffer1, 6, buffer0, hex0_len, false)));
    random_alnum(buffer1, sizeof(buffer1));
    assert(!strcmp("....", hexmem(buffer1, 5, buffer0, hex0_len, false)));
    random_alnum(buffer1, sizeof(buffer1));
    assert(!strcmp("...", hexmem(buffer1, 4, buffer0, hex0_len, false)));
    random_alnum(buffer1, sizeof(buffer1));
    assert(!strcmp("..", hexmem(buffer1, 3, buffer0, hex0_len, false)));
    random_alnum(buffer1, sizeof(buffer1));
    assert(!strcmp(".", hexmem(buffer1, 2, buffer0, hex0_len, false)));
    random_alnum(buffer1, sizeof(buffer1));
    assert(!strcmp("", hexmem(buffer1, 1, buffer0, hex0_len, false)));
}

int main(int argc, char *argv[]) {
    for (int i = 0; i < 100; i++)
        TEST_random_alnum();

    TEST_cstring_array();

    TEST_hexmem();
}

#endif /* __TEST_MISC */
