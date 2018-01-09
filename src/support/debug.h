/*********************************************************************************/
/*  Copyright 2017 Barcelona Supercomputing Center                               */
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
/*  along with DLB.  If not, see <http://www.gnu.org/licenses/>.                 */
/*********************************************************************************/

#ifndef DEBUG_H
#define DEBUG_H

#include "support/types.h"
#include "support/options.h"

#include <stdlib.h>
#include <stdio.h>

#ifdef MPI_LIB
#include "LB_MPI/process_MPI.h"
#endif

void debug_init(const options_t *options);
void vb_print(FILE *fp, const char *prefix, const char *fmt, ...);
void print_backtrace(void);
void dlb_clean(void);

extern bool             vb_werror;
extern verbose_opts_t   vb_opts;


#define fatal(...) \
    do { \
        vb_print(stderr, "DLB PANIC", __VA_ARGS__); \
        dlb_clean(); \
        abort(); \
    } while(0)

#define warning(...) \
    do { \
        if (!vb_werror) { vb_print(stderr, "DLB WARNING", __VA_ARGS__); } \
        else { fatal(__VA_ARGS__); } \
    } while(0)

#define info(...) \
    vb_print(stdout, "DLB", __VA_ARGS__)

#ifdef MPI_LIB
// verbose0 macros will print only if rank == 0 in MPI
#define fatal0(...) \
    do { \
        if (_mpi_rank <= 0) { fatal(__VA_ARGS__); } \
        else { dlb_clean(); abort(); } \
    } while(0)

#define warning0(...) \
    do { \
        if (_mpi_rank <= 0) { warning(__VA_ARGS__); } \
    } while(0)

#define info0(...) \
    do { \
        if (_mpi_rank <= 0) { info(__VA_ARGS__); } \
    } while(0)

#else /* MPI_LIB */
// if MPI is not preset, do the same as the generic ones
#define fatal0(...)     fatal(__VA_ARGS__)
#define warning0(...)   warning(__VA_ARGS__)
#define info0(...)      info(__VA_ARGS__)
#endif /* MPI_LIB */


#define fatal_cond(cond, ...) \
    do { \
        if (cond) { fatal(__VA_ARGS__); } \
    } while(0)

#define fatal_cond0(cond, ...) \
    do { \
        if (cond) { fatal0(__VA_ARGS__); } \
    } while(0)

#define fatal_cond_strerror(cond) \
    do { \
        int _error = cond; \
        if (_error) fatal(#cond ":%s", strerror(_error)); \
    } while(0)


#ifdef DEBUG_VERSION
#define verbose(flag, ...) \
    do { \
        if ( flag & vb_opts & VB_API )          { vb_print(stdout, "DLB API", __VA_ARGS__); } \
        else if ( flag & vb_opts & VB_MICROLB ) { vb_print(stdout, "DLB MICROLB", __VA_ARGS__); } \
        else if ( flag & vb_opts & VB_SHMEM )   { vb_print(stdout, "DLB SHMEM", __VA_ARGS__); } \
        else if ( flag & vb_opts & VB_MPI_API ) { vb_print(stdout, "DLB MPI API", __VA_ARGS__); } \
        else if ( flag & vb_opts & VB_MPI_INT ) { vb_print(stdout, "DLB MPI INT", __VA_ARGS__); } \
        else if ( flag & vb_opts & VB_STATS )   { vb_print(stdout, "DLB STATS", __VA_ARGS__); } \
        else if ( flag & vb_opts & VB_DROM )    { vb_print(stdout, "DLB DROM", __VA_ARGS__); } \
        else if ( flag & vb_opts & VB_ASYNC )   { vb_print(stdout, "DLB ASYNC", __VA_ARGS__); } \
        else if ( flag & vb_opts & VB_OMPT )    { vb_print(stdout, "DLB OMPT", __VA_ARGS__); } \
    } while(0)
#else
#define verbose(flag, ...)
#endif

#ifdef DEBUG_VERSION
#define DLB_DEBUG(f) f
#else
#define DLB_DEBUG(f)
#endif

#ifdef DEBUG_VERSION
#define ensure(cond, ...) \
    do { \
        if (!(cond)) { fatal(__VA_ARGS__); } \
    } while(0)

#define static_ensure(cond, ...) \
    do { \
        enum { assert_static__ = 1/(cond) };\
    } while (0)

#else
#define ensure(cond, ...)
#define static_ensure(cond, ...)
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
