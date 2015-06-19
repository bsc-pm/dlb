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

void fatal0 ( const char *fmt, ... );
void fatal ( const char *fmt, ... );
void warning0 ( const char *fmt, ... );
void warning ( const char *fmt, ... );
void warningT ( const char *fmt, ... );

#ifndef QUIET_MODE
void verbose0 ( const char *fmt, ... );
void verbose ( const char *fmt, ... );
void verboseT ( const char *fmt, ... );
#else
#define verbose0(fmt, ...)
#define verbose(fmt, ...)
#define verboseT(fmt, ...)
#endif

#ifdef debugBasicInfo
void debug_basic_info0 ( const char *fmt, ... );
void debug_basic_info ( const char *fmt, ... );
#else
#define debug_basic_info0(fmt, ...)
#define debug_basic_info(fmt, ...)
#endif

#ifdef debugConfig
void debug_config ( const char *fmt, ... );
#else
#define debug_config(fmt, ...)
#endif

#ifdef debugInOut
void debug_inout ( const char *fmt, ... );
#else
#define debug_inout(fmt, ...)
#endif

#ifdef debugInOutMPI
void debug_inout_MPI ( const char *fmt, ... );
#else
#define debug_inout_MPI(fmt, ...)
#endif

#ifdef debugBlockingMPI
void debug_blocking_MPI ( const char *fmt, ... );
#else
#define debug_blocking_MPI(fmt, ...)
#endif

#ifdef debugLend
void debug_lend ( const char *fmt, ... );
#else
#define debug_lend(fmt, ...)
#endif

#ifdef debugSharedMem
void debug_shmem ( const char *fmt, ... );
#else
#define debug_shmem(fmt, ...)
#endif

#define fatal_cond(cond, ...) if ( cond ) fatal(__VA_ARGS__);
#define fatal_cond0(cond, ...) if ( cond ) fatal0(__VA_ARGS__);

#ifdef DEBUG_VERSION
#define DLB_DEBUG(f) f
#define ensure(cond, ...) if ( !(cond) ) fatal(__VA_ARGS__);
#else
#define DLB_DEBUG(f)
#define ensure(cond, ...)
#endif

void print_backtrace ( void );

#endif /* DEBUG_H */
