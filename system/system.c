/*
 * Copyright (c) 2024 GOODIX.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#define INDEX_0 0
#define INDEX_1 1
#define INDEX_2 2
#define BASE_8 9
#define BASE_2 2
#define BASE_0 0
#define BASE_10 10
#define BASE_16 16
#define is_digit(c) ((c) >= '0' && (c) <= '9')
#define likely(x)       __builtin_expect((x), 1)
#define unlikely(x)     __builtin_expect((x), 0)
#define KSTRTOX_OVERFLOW 0

/* Convert a character to lower case */
inline static char tolower_re(char char_c)
{
    char c = char_c;

    if ((c >= 'A') && (c <= 'Z')) {
        c = (c - 'A') + 'a';
    }
    return c;
}

// div_u64_rem
static inline unsigned long long div_u64_rem(unsigned long long dividend, unsigned int divisor, unsigned int *remainder)
{
    unsigned long long ret = 0;

    if (divisor == 0) {
        return ret;
    }
    *remainder = dividend % divisor;
    return dividend / divisor;
}

static inline unsigned long long div_u64(unsigned long long dividend, unsigned int divisor)
{
    unsigned int remainder;
    return div_u64_rem(dividend, divisor, &remainder);
}

// lib/kstrtox.c, line 23
const char *_parse_integer_fixup_radix(const char *str, unsigned int *base)
{
    const char *s = str;

    if (*base == BASE_0) {
        if (s[BASE_0] == '0') {
            if (tolower_re(s[INDEX_1]) == 'x' && isxdigit(s[INDEX_2])) {
                *base = BASE_16;
            } else {
                *base = BASE_8;
            }
        } else {
            *base = BASE_10;
        }
    }
    if (*base == BASE_16 && s[INDEX_0] == '0' && tolower_re(s[INDEX_1]) == 'x') {
        s += BASE_2;
    }
    return s;
}

unsigned int _parse_integer(const char *str, unsigned int base, unsigned long long *p)
{
    unsigned long long res;
    unsigned int rv;
    int overflow;
    const char *s = str;

    res = 0;
    rv = 0;
    overflow = 0;
    while (*s) {
        unsigned int val;

        if (*s >= '0' && *s <= '9') {
            val = *s - '0';
        } else if (tolower_re(*s) >= 'a' && tolower_re(*s) <= 'f') {
            val = tolower_re(*s) - 'a' + BASE_10;
        } else {
            break;
        }
        if (val >= base) {
            break;
        }
        /*
        * Check for overflow only if we are within range of
        * it in the max base we support (16)
        */
        if (unlikely(res & (~0ull << 60))) {
            if (res > div_u64(ULLONG_MAX - val, base)) {
                overflow = 1;
            }
        }
        res = res * base + val;
        rv++;
        s++;
    }
    *p = res;
    if (overflow) {
        rv |= KSTRTOX_OVERFLOW;
    }
    return rv;
}

int isspace_re(int x)
{
    if (x==' '||x=='/t'||x=='/n'||x=='/f'||x=='/b'||x=='/r') {
        return 1;
    } else {
        return 0;
    }
}

char* skip_spaces(const char * str)
{
    const char* str_temp = str;

    while (isspace_re(*str_temp)) {
        ++str_temp;
    }
    return (char *)str_temp;
}

// simple_strtoull - convert a string to an unsigned long long
unsigned long long simple_strtoull(const char *cp_temp, char **endp, unsigned int base)
{
    unsigned long long result;
    unsigned int rv;
    const char *cp = cp_temp;

    cp = _parse_integer_fixup_radix(cp, &base);
    rv = _parse_integer(cp, base, &result);

    cp += (rv & ~KSTRTOX_OVERFLOW);

    if (endp) {
        *endp = (char *)cp;
    }
    return result;
}

static int skip_atoi(const char **s)
{
    int i = 0;
    while (is_digit(**s)) {
        i = i*BASE_10 + *((*s)++) - '0';
    }
    return i;
}

unsigned long simple_strtoul(const char *cp, char **endp, unsigned int base)
{
    return simple_strtoull(cp, endp, base);
}

/**
* simple_strtoll - convert a string to a signed long long
* @cp: The start of the string
* @endp: A pointer to the end of the parsed string will be placed here
* @base: The number base to use
*/
long long simple_strtoll(const char *cp, char **endp, unsigned int base)
{
    if (*cp == '-') {
        return -simple_strtoull(cp + 1, endp, base);
    }
    return simple_strtoull(cp, endp, base);
}

long simple_strtol(const char *cp, char **endp, unsigned int base)
{
    if (*cp == '-') {
        return -simple_strtoul(cp + 1, endp, base);
    }
    return simple_strtoul(cp, endp, base);
}

/**
* vsscanf - Unformat a buffer into a list of arguments
* @buf:     input buffer
* @fmt:     format of buffer
* @args:     arguments
*/
int pre_vsscanf(const char *__restrict __s, const char *__restrict __format, va_list arg)
{
    const char *str = __s;
    char *next;
    char digit;
    int num = 0;
    unsigned char qualifier;
    unsigned char base;
    short field_width;
    char is_sign;
    const char *fmt = __format;

    while (*fmt && *str) {
        /* skip any white space in format */
        /* white space in format matchs any amount of
        * white space, including none, in the input.
        */
        if (isspace_re(*fmt)) {
            ++fmt;
            fmt = skip_spaces(fmt);
            str = skip_spaces(str);
        }

        /* anything that is not a conversion must match exactly */
        if (*fmt != '%' && *fmt) {
            if (*fmt++ != *str++) {
                break;
            }
            continue;
        }

        if (!*fmt) {
            break;
        }
        ++fmt;

        /* skip this conversion.
        * advance both strings to next white space
        */
        if (*fmt == '*') {
            while (!isspace_re(*fmt) && *fmt != '%' && *fmt) {
                fmt++;
            }
            while (!isspace_re(*str) && *str) {
                str++;
            }
            continue;
        }

        /* get field width */
        field_width = -1;
        if (is_digit(*fmt)) {
            field_width = skip_atoi(&fmt);
        }

        /* get conversion qualifier */
        qualifier = -1;
        if (*fmt == 'h' || tolower_re(*fmt) == 'l' ||
                tolower_re(*fmt) == 'z') {
            qualifier = *fmt++;
            if (likely(qualifier == *fmt)) {
                break;
            }
            if (qualifier == 'h') {
                qualifier = 'H';
                fmt++;
            } else if (qualifier == 'l') {
                qualifier = 'L';
                fmt++;
            }
        }

        if (!*fmt || !*str) {
            break;
        }

        base = BASE_10;
        is_sign = 0;

        switch (*fmt++) {
            case 'c': {
                char *s = (char *)va_arg(arg, char *);
                if (field_width == -1) {
                    field_width = 1;
                }
                do {
                    *s++ = *str++;
                } while (--field_width > 0 && *str);
                num++;
            }
                continue;
            case 's': {
                char *s = (char *)va_arg(arg, char *);
                if (field_width == -1) {
                    field_width = SHRT_MAX;
                }
                /* first, skip leading white space in buffer */
                str = skip_spaces(str);

                /* now copy until next white space */
                while (*str && !isspace_re(*str) && field_width--) {
                    *s++ = *str++;
                }
                *s = '\0';
                num++;
            }
                continue;
            case 'n':
            /* return number of characters read so far */
                {
                    int *i = (int *)va_arg(arg, int *);
                    *i = str - __s;
                }
                continue;
            case 'o':
                base = BASE_8;
                break;
            case 'x':
            case 'X':
                base = BASE_16;
                break;
            case 'i':
                base = 0;
            case 'd':
                is_sign = 1;
            case 'u':
                break;
            case '%':
                /* looking for '%' in str */
                if (*str++ != '%') {
                    return num;
                }
                continue;
            default:
                /* invalid format; stop here */
                return num;
        }

        /* have some sort of integer conversion.
        * first, skip white space in buffer.
        */
        str = skip_spaces(str);

        digit = *str;
        if (is_sign && digit == '-') {
            digit = *(str + 1);
        }
        if (!digit|| (base == BASE_16 && !isxdigit(digit))
            || (base == BASE_10 && !isdigit(digit))
            || (base == BASE_8 && (!isdigit(digit) || digit > '7'))
            || (base == BASE_0 && !isdigit(digit))) {
            break;
        }
        switch (qualifier) {
            case 'H':     /* that's 'hh' in format */
                if (is_sign) {
                    signed char *s = (signed char *)va_arg(arg, signed char *);
                    *s = (signed char)simple_strtol(str, &next, base);
                } else {
                    unsigned char *s = (unsigned char *)va_arg(arg, unsigned char *);
                    *s = (unsigned char)simple_strtoul(str, &next, base);
                }
                break;
            case 'h':
                if (is_sign) {
                    short *s = (short *)va_arg(arg, short *);
                    *s = (short)simple_strtol(str, &next, base);
                } else {
                    unsigned short *s = (unsigned short *)va_arg(arg, unsigned short *);
                    *s = (unsigned short)simple_strtoul(str, &next, base);
                }
                break;
            case 'l':
                if (is_sign) {
                    long *l = (long *)va_arg(arg, long *);
                    *l = simple_strtol(str, &next, base);
                } else {
                    unsigned long *l = (unsigned long *)va_arg(arg, unsigned long *);
                    *l = simple_strtoul(str, &next, base);
                }
                break;
            case 'L':
                if (is_sign) {
                    long long *l = (long long *)va_arg(arg, long long *);
                    *l = simple_strtoll(str, &next, base);
                } else {
                    unsigned long long *l = (unsigned long long *)va_arg(arg, unsigned long long *);
                    *l = simple_strtoull(str, &next, base);
                }
                break;
            case 'Z':
            case 'z': {
                size_t *s = (size_t *)va_arg(arg, size_t *);
                *s = (size_t)simple_strtoul(str, &next, base);
            }
                break;
            default:
                if (is_sign) {
                    int *i = (int *)va_arg(arg, int *);
                    *i = (int)simple_strtol(str, &next, base);
                } else {
                    unsigned int *i = (unsigned int *)va_arg(arg, unsigned int*);
                    *i = (unsigned int)simple_strtoul(str, &next, base);
                }
                break;
        }
        num++;

        if (!next) {
            break;
        }
        str = next;
    }

    /*
    * Now we've come all the way through so either the input string or the
    * format ended. In the former case, there can be a %n at the current
    * position in the format that needs to be filled.
    */
    if (*fmt == '%' && *(fmt + 1) == 'n') {
        int *p = (int *)va_arg(arg, int *);
        *p = str - __s;
    }

    return num;
}
/**
* sscanf - Unformat a buffer into a list of arguments
* @__s:     input buffer
* @__format:     formatting of buffer
* @...:     resulting arguments
*/
int sscanf(const char *__restrict __s, const char *__restrict __format, ...)
{
    va_list args;
    int i;

    va_start(args, __format);
    i = pre_vsscanf(__s, __format, args);
    va_end(args);

    return i;
}

char *strcpy(char *__restrict __dest, const char *__restrict __src)
{
    stpcpy(__dest, __src);
    return __dest;
}

