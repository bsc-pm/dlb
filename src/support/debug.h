/*********************************************************************************/
/*  Copyright 2015 Barcelona Supercomputing Center                               */
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

#include <stdlib.h>
#include <stdio.h>
#include "support/types.h"
#include "support/globals.h"

void debug_init(void);
void vb_print(FILE *fp, const char *prefix, const char *fmt, ...);
void print_backtrace(void);


extern verbose_opts_t vb_opts;

#define fatal0(...) \
    if (_mpi_rank <= 0) { vb_print(stderr, "DLB PANIC", __VA_ARGS__); } \
    abort();

#define fatal(...) \
    vb_print(stderr, "DLB PANIC", __VA_ARGS__); \
    abort();

#define warning0(...) \
    if (_mpi_rank <= 0) { vb_print(stderr, "DLB WARNING", __VA_ARGS__); }

#define warning(...) \
    vb_print(stderr, "DLB WARNING", __VA_ARGS__);

#define info0(...) \
    if (_mpi_rank <= 0) { vb_print(stdout, "DLB", __VA_ARGS__); }

#define info(...) \
    vb_print(stdout, "DLB", __VA_ARGS__);

#define fatal_cond(cond, ...) if (cond) { fatal(__VA_ARGS__); }
#define fatal_cond0(cond, ...) if (cond) { fatal0(__VA_ARGS__); }


#ifdef DEBUG_VERSION
#define verbose(flag, ...) \
    if ( flag & vb_opts & VB_API )          { vb_print(stdout, "DLB API", __VA_ARGS__); } \
    else if ( flag & vb_opts & VB_MICROLB ) { vb_print(stdout, "DLB MICROLB", __VA_ARGS__); } \
    else if ( flag & vb_opts & VB_SHMEM )   { vb_print(stdout, "DLB SHMEM", __VA_ARGS__); } \
    else if ( flag & vb_opts & VB_MPI_API ) { vb_print(stdout, "DLB MPI API", __VA_ARGS__); } \
    else if ( flag & vb_opts & VB_MPI_INT ) { vb_print(stdout, "DLB MPI INT", __VA_ARGS__); } \
    else if ( flag & vb_opts & VB_STATS )   { vb_print(stdout, "DLB STATS", __VA_ARGS__); } \
    else if ( flag & vb_opts & VB_DROM )    { vb_print(stdout, "DLB DROM", __VA_ARGS__); }
#else
#define verbose(flag, ...)
#endif

#ifdef DEBUG_VERSION
#define DLB_DEBUG(f) f
#else
#define DLB_DEBUG(f)
#endif

#ifdef DEBUG_VERSION
#define ensure(cond, ...) if (!(cond)) { fatal(__VA_ARGS__); }
#define static_ensure(cond, ...) \
    do { enum { assert_static__ = 1/(cond) };} while (0)
#else
#define ensure(cond, ...)
#define static_ensure(cond, ...)
#endif

#endif /* DEBUG_H */
