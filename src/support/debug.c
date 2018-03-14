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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "support/debug.h"

#include "support/options.h"
#include "LB_comm/comm_lend_light.h"
#include "LB_comm/shmem.h"
#include "LB_comm/shmem_async.h"
#include "LB_comm/shmem_barrier.h"
#include "LB_comm/shmem_cpuinfo.h"
#include "LB_comm/shmem_procinfo.h"
#include "apis/DLB_interface.h"

#include <sys/types.h>
#include <sys/syscall.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <stdarg.h>
#include <time.h>

#ifdef HAVE_EXECINFO_H
#include <execinfo.h>
#endif

verbose_opts_t vb_opts = VB_CLEAR;

enum { VBFORMAT_LEN = 32 };
static verbose_fmt_t vb_fmt;
static char fmt_str[VBFORMAT_LEN];
static bool werror = false;

void debug_init(const options_t *options) {
    werror = options->debug_opts & DBG_WERROR;
    vb_opts = options->verbose;
    vb_fmt = options->verbose_fmt;

    int i = 0;
    if ( vb_fmt & VBF_NODE ) {
        char hostname[HOST_NAME_MAX];
        gethostname( hostname, HOST_NAME_MAX );
        i += sprintf( &fmt_str[i], "%s:", hostname);
    }
    if ( vb_fmt & VBF_PID ) { i += sprintf( &fmt_str[i], "%d:", getpid()); }
#ifdef MPI_LIB
    if ( vb_fmt & VBF_MPINODE ) { i += sprintf( &fmt_str[i], "%d:", _node_id); }
    if ( vb_fmt & VBF_MPIRANK ) { i += sprintf( &fmt_str[i], "%d:", _mpi_rank); }
#endif

    // Remove last separator ':' if fmt_str is not empty
    if ( i !=0 ) {
        fmt_str[i-1] = '\0';
    }
}

static void vprint(FILE *fp, const char *prefix, const char *fmt, va_list list) {

    char timestamp[32];
    if (vb_fmt & VBF_TSTAMP) {
        time_t t = time(NULL);
        struct tm *tm = localtime(&t);
        strftime(timestamp, sizeof(timestamp), "[%Y-%m-%dT%T] ", tm);
    }

    // Print prefix and object identifier
    if (vb_fmt & VBF_THREAD) {
        fprintf(fp, "%s%s[%s:%ld]: ",
                vb_fmt & VBF_TSTAMP ? timestamp : "",
                prefix,
                fmt_str,
                syscall(SYS_gettid));
    } else {
        fprintf(fp, "%s%s[%s]: ",
                vb_fmt & VBF_TSTAMP ? timestamp : "",
                prefix,
                fmt_str);
    }

    // Print va_list
    vfprintf(fp, fmt, list);
    fputc('\n', fp);
}

static void __attribute__((__noreturn__)) vfatal(const char *fmt, va_list list) {
    vprint(stderr, "DLB PANIC", fmt, list);
    dlb_clean();
    abort();
}

void fatal(const char *fmt, ...) {
    va_list list;
    va_start(list, fmt);
    vfatal(fmt, list);
    va_end(list);
}

void fatal0(const char *fmt, ...) {
#ifdef MPI_LIB
    if (_mpi_rank <= 0) {
#endif
        va_list list;
        va_start(list, fmt);
        vfatal(fmt, list);
        va_end(list);
#ifdef MPI_LIB
    } else {
        dlb_clean();
        abort();
    }
#endif
}

static void vwarning(const char *fmt, va_list list) {
    vprint(stderr, "DLB WARNING", fmt, list);
}

void warning(const char *fmt, ...) {
    va_list list;
    va_start(list, fmt);
    if (!werror) vwarning(fmt, list);
    else vfatal(fmt, list);
    va_end(list);
}

void warning0(const char *fmt, ...) {
#ifdef MPI_LIB
    if (_mpi_rank <= 0) {
#endif
        va_list list;
        va_start(list, fmt);
        if (!werror) vwarning(fmt, list);
        else vfatal(fmt, list);
        va_end(list);
#ifdef MPI_LIB
    }
#endif
}

static void vinfo(const char *fmt, va_list list) {
    vprint(stdout, "DLB", fmt, list);
}

void info(const char *fmt, ...) {
    va_list list;
    va_start(list, fmt);
    vinfo(fmt, list);
    va_end(list);
}

void info0(const char *fmt, ...) {
#ifdef MPI_LIB
    if (_mpi_rank <= 0) {
#endif
        va_list list;
        va_start(list, fmt);
        vinfo(fmt, list);
        va_end(list);
#ifdef MPI_LIB
    }
#endif
}

#undef verbose
void verbose(verbose_opts_t flag, const char *fmt, ...) {
    va_list list;
    va_start(list, fmt);
    if      (vb_opts & flag & VB_API)     { vprint(stdout, "DLB API", fmt, list); }
    else if (vb_opts & flag & VB_MICROLB) { vprint(stdout, "DLB MICROLB", fmt, list); }
    else if (vb_opts & flag & VB_SHMEM)   { vprint(stdout, "DLB SHMEM", fmt, list); }
    else if (vb_opts & flag & VB_MPI_API) { vprint(stdout, "DLB MPI API", fmt, list); }
    else if (vb_opts & flag & VB_MPI_INT) { vprint(stdout, "DLB MPI INT", fmt, list); }
    else if (vb_opts & flag & VB_STATS)   { vprint(stdout, "DLB STATS", fmt, list); }
    else if (vb_opts & flag & VB_DROM)    { vprint(stdout, "DLB DROM", fmt, list); }
    else if (vb_opts & flag & VB_ASYNC)   { vprint(stdout, "DLB ASYNC", fmt, list); }
    else if (vb_opts & flag & VB_OMPT)    { vprint(stdout, "DLB OMPT", fmt, list); }
    va_end(list);
}

void print_backtrace(void) {
#ifdef HAVE_EXECINFO_H
    void* trace_ptrs[100];
    int count = backtrace( trace_ptrs, 100 );
    char** func_names = backtrace_symbols( trace_ptrs, count );
    fprintf( stderr, "+--------------------------------------\n" );

    // Print the stack trace
    int i;
    for( i = 0; i < count; i++ ) {
        fprintf( stderr, "| %s\n", func_names[i] );
    }

    // Free the string pointers
    free( func_names );
    fprintf( stderr, "+--------------------------------------\n" );
#else
    fprintf( stderr, "+--------------------------------------\n" );
    fprintf( stderr, "  Backtrace not supported\n") ;
    fprintf( stderr, "+--------------------------------------\n" );
#endif
}

void dlb_clean(void) {
    // Best effort, finalize current pid on all shmems
    pid_t pid = getpid();
    const options_t *options = get_global_options();
    const char *shmem_key = options ? options->shm_key : NULL;

    if (shmem_cpuinfo__exists()) {
        shmem_cpuinfo__finalize(pid, shmem_key);
    }
    else if (shmem_exists("cpuinfo", shmem_key)) {
        shmem_destroy("cpuinfo", shmem_key);
    }

    if (shmem_procinfo__exists()) {
        shmem_procinfo__finalize(pid, false, shmem_key);
    }
    else if (shmem_exists("procinfo", shmem_key)) {
        shmem_destroy("procinfo", shmem_key);
    }

    shmem_barrier_finalize();
    shmem_async_finalize(pid);
    finalize_comm();
}
