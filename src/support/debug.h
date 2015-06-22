/*************************************************************************************/
/*      Copyright 2014 Barcelona Supercomputing Center                               */
/*                                                                                   */
/*      This file is part of the DLB library.                                        */
/*                                                                                   */
/*      DLB is free software: you can redistribute it and/or modify                  */
/*      it under the terms of the GNU Lesser General Public License as published by  */
/*      the Free Software Foundation, either version 3 of the License, or            */
/*      (at your option) any later version.                                          */
/*                                                                                   */
/*      DLB is distributed in the hope that it will be useful,                       */
/*      but WITHOUT ANY WARRANTY; without even the implied warranty of               */
/*      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                */
/*      GNU Lesser General Public License for more details.                          */
/*                                                                                   */
/*      You should have received a copy of the GNU Lesser General Public License     */
/*      along with DLB.  If not, see <http://www.gnu.org/licenses/>.                 */
/*************************************************************************************/

#ifndef DEBUG_H
#define DEBUG_H

#include <stdlib.h>
#include <stdio.h>

typedef enum VerboseOptions {
    VB_CLEAR    = 0,
    VB_API      = 1 << 0,
    VB_MICROLB  = 1 << 1,
    VB_SHMEM    = 1 << 2,
    VB_MPI_API  = 1 << 3,
    VB_MPI_INT  = 1 << 4,
    VB_STATS    = 1 << 5,
    VB_DROM     = 1 << 6
} verbose_opts_t;

typedef enum VerboseFormat {
    VBF_CLEAR   = 0,
    VBF_NODE    = 1 << 0,
    VBF_PID     = 1 << 1,
    VBF_MPINODE = 1 << 2,
    VBF_MPIRANK = 1 << 3,
    VBF_THREAD  = 1 << 4
} verbose_fmt_t;


void debug_init();
void print(FILE *fp, const char *prefix, const char *fmt, ...);
void print_backtrace(void);


// FIXME: we don't want to include globals.h
// because it incurs into a circular dependency
extern int _mpi_rank;
extern verbose_opts_t vb_opts;

#define fatal0(...) \
    if (_mpi_rank <= 0) { print(stderr, "DLB PANIC", __VA_ARGS__); } \
    abort();

#define fatal(...) \
    print(stderr, "DLB PANIC", __VA_ARGS__); \
    abort();

#define warning0(...) \
    if (_mpi_rank <= 0) { print(stderr, "DLB WARNING", __VA_ARGS__); }

#define warning(...) \
    print(stderr, "DLB WARNING", __VA_ARGS__);

#define info0(...) \
    if (_mpi_rank <= 0) { print(stdout, "DLB", __VA_ARGS__); }

#define info(...) \
    print(stdout, "DLB", __VA_ARGS__);

#define fatal_cond(cond, ...) if (cond) { fatal(__VA_ARGS__); }
#define fatal_cond0(cond, ...) if (cond) { fatal0(__VA_ARGS__); }


#ifdef DEBUG_VERSION
#define verbose(flag, ...) \
    if ( flag & vb_opts & VB_API )          { print(stdout, "DLB_API", __VA_ARGS__); } \
    else if ( flag & vb_opts & VB_MICROLB ) { print(stdout, "DLB MICROLB", __VA_ARGS__); } \
    else if ( flag & vb_opts & VB_SHMEM )   { print(stdout, "DLB SHMEM", __VA_ARGS__); } \
    else if ( flag & vb_opts & VB_MPI_API ) { print(stdout, "DLB MPI API", __VA_ARGS__); } \
    else if ( flag & vb_opts & VB_MPI_INT ) { print(stdout, "DLB MPI INT", __VA_ARGS__); } \
    else if ( flag & vb_opts & VB_STATS )   { print(stdout, "DLB STATS", __VA_ARGS__); } \
    else if ( flag & vb_opts & VB_DROM )    { print(stdout, "DLB DROM", __VA_ARGS__); }
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
#else
#define ensure(cond, ...)
#endif

#endif /* DEBUG_H */
