/*********************************************************************************/
/*  Copyright 2009-2021 Barcelona Supercomputing Center                          */
/*                                                                               */
/*  This file is part of the DLB library.                                        */
/*                                                                               */
/*  DLB is free software: you can redistribute it and/or modify                  */
/*  it under the terms of the GNU Lesser General Public License as published by  */
/*  the Free Software Foundation, either version 3 of the License, or            */
/*  (at your option) any later version.                                          */
/*                                                                               */
/*  DLB is distributed in the hope that it will be useful,                       */
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of               */
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                */
/*  GNU Lesser General Public License for more details.                          */
/*                                                                               */
/*  You should have received a copy of the GNU Lesser General Public License     */
/*  along with DLB.  If not, see <https://www.gnu.org/licenses/>.                */
/*********************************************************************************/

#ifndef DEBUG_H
#define DEBUG_H

#include "support/dlb_common.h"
#include "support/options.h"
#include "support/types.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct print_buffer {
    char *addr;
    size_t size;
    size_t len;
} print_buffer_t;

void debug_init(const options_t *options);
void fatal(const char *fmt, ...)    __attribute__ ((format (printf, 1, 2),__noreturn__));
void fatal0(const char *fmt, ...)   __attribute__ ((format (printf, 1, 2),__noreturn__));
void warning(const char *fmt, ...)  __attribute__ ((format (printf, 1, 2)));
void warning0(const char *fmt, ...) __attribute__ ((format (printf, 1, 2)));
void info(const char *fmt, ...)     __attribute__ ((format (printf, 1, 2)));
void info0(const char *fmt, ...)    __attribute__ ((format (printf, 1, 2)));
void info0_force_print(const char *fmt, ...)    __attribute__ ((format (printf, 1, 2)));
void verbose(verbose_opts_t flag, const char *fmt, ...) __attribute__ ((format (printf, 2, 3)));
void verbose0(verbose_opts_t flag, const char *fmt, ...) __attribute__ ((format (printf, 2, 3)));
void print_backtrace(void);
void dlb_clean(void);
void warn_error(int error);
void printbuffer_init(print_buffer_t *buffer);
void printbuffer_destroy(print_buffer_t *buffer);
void printbuffer_append(print_buffer_t *buffer, const char *line);
void printbuffer_append_no_newline(print_buffer_t *buffer, const char *text);

extern verbose_opts_t   vb_opts;

#define fatal_cond(cond, ...) \
    do { \
        if (unlikely(cond)) { fatal(__VA_ARGS__); } \
    } while(0)

#define fatal_cond0(cond, ...) \
    do { \
        if (unlikely(cond)) { fatal0(__VA_ARGS__); } \
    } while(0)

#define fatal_cond_strerror(cond) \
    do { \
        int _error = cond; \
        if (unlikely(_error)) fatal(#cond ":%s", strerror(_error)); \
    } while(0)

#define verbose(flag, ...) \
    do { \
        if (unlikely(vb_opts != VB_CLEAR)) { verbose(flag, __VA_ARGS__); } \
    } while(0)

#define verbose0(flag, ...) \
    do { \
        if (unlikely(vb_opts != VB_CLEAR)) { verbose0(flag, __VA_ARGS__); } \
    } while(0)


#ifdef DEBUG_VERSION
#define DLB_DEBUG(stmt) do { stmt; } while(0)
#else
#define DLB_DEBUG(stmt) do { (void)0; } while(0)
#endif

#ifdef DEBUG_VERSION
#define debug_warning(...) warning(__VA_ARGS__)

#define ensure(cond, ...) \
    do { \
        if (likely(cond)) ; else { fatal(__VA_ARGS__); } \
    } while(0)


#if __STDC_VERSION__ >= 202311L
    #define static_ensure(cond, msg) static_assert(cond, msg)
#elif __STDC_VERSION__ >= 201112L
    #define static_ensure(cond, msg) _Static_assert(cond, msg)
#else
    #define static_ensure(cond, msg) \
        do { \
            enum { assert_static__ = 1 / (!!(cond)) }; \
        } while (0)
#endif

#else
#define debug_warning(...)       do { (void)0; } while(0)
#define ensure(cond, ...)        do { (void)0; } while(0)
#define static_ensure(cond, ...) do { (void)0; } while(0)
#endif

/* Colors */

#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

#endif /* DEBUG_H */
