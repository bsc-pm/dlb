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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "support/debug.h"

#include "apis/dlb_errors.h"
#include "support/options.h"
#include "support/mask_utils.h"
#include "LB_comm/comm_lend_light.h"
#include "LB_comm/shmem.h"
#include "LB_comm/shmem_async.h"
#include "LB_comm/shmem_barrier.h"
#include "LB_comm/shmem_cpuinfo.h"
#include "LB_comm/shmem_procinfo.h"
#include "LB_comm/shmem_talp.h"
#include "LB_core/spd.h"

#ifdef MPI_LIB
#include "LB_MPI/process_MPI.h"
#endif

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

verbose_opts_t vb_opts = VB_UNDEF;

enum { VBFORMAT_LEN = 128 };
static verbose_fmt_t vb_fmt;
static char fmt_str[VBFORMAT_LEN];
static bool quiet = false;
static bool silent = false;
static bool werror = false;
static pthread_mutex_t dlb_clean_mutex = PTHREAD_MUTEX_INITIALIZER;

void debug_init(const options_t *options) {
    quiet = options->quiet;
    silent = options->silent;
    werror = options->debug_opts & DBG_WERROR;
    vb_opts = options->verbose;
    vb_fmt = options->verbose_fmt;

    int i = 0;
    if ( vb_fmt & VBF_NODE ) {
        char hostname[VBFORMAT_LEN/2];
        gethostname( hostname, VBFORMAT_LEN/2);
        i += sprintf( &fmt_str[i], "%s:", hostname);
    }
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
    // Write timestamp
    enum { TIMESTAMP_MAX_SIZE = 32 };
    char timestamp[TIMESTAMP_MAX_SIZE];
    if (vb_fmt & VBF_TSTAMP) {
        time_t t = time(NULL);
        struct tm *tm = localtime(&t);
        strftime(timestamp, TIMESTAMP_MAX_SIZE, "[%Y-%m-%dT%T] ", tm);
    } else {
        timestamp[0] = '\0';
    }

    // Write spid
    enum { SPID_MAX_SIZE = 16 };
    char spid[SPID_MAX_SIZE];
    if (vb_fmt & VBF_SPID && thread_spd) {
        snprintf(spid, SPID_MAX_SIZE, ":%d", thread_spd->id);
    } else {
        spid[0] = '\0';
    }

    // Write thread id
    enum { THREADID_MAX_SIZE = 24 };
    char threadid[THREADID_MAX_SIZE];
    if (vb_fmt & VBF_THREAD) {
        snprintf(threadid, THREADID_MAX_SIZE, ":%ld", syscall(SYS_gettid));
    } else {
        threadid[0] = '\0';
    }

    // Allocate message in an intermediate buffer and print in one function
    char *msg;
    vasprintf(&msg, fmt, list);
    fprintf(fp, "%s%s[%s%s%s]: %s\n", timestamp, prefix, fmt_str, spid, threadid, msg);
    free(msg);
}

static void __attribute__((__noreturn__)) vfatal(const char *fmt, va_list list) {
    /* Parse --silent option if fatal() was invoked before init */
    if (unlikely(vb_opts == VB_UNDEF)) {
        /* If fatal() was invoked before debug_init, we want to parse the
         * --silent option but ensuring that parsing the options does not cause
         *  a recursive fatal error */
        vb_opts = VB_CLEAR;
        options_parse_entry("--silent", &silent);
    }

    if (!silent) {
        vprint(stderr, "DLB PANIC", fmt, list);
    }
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
    /* Parse --silent option if warning() was invoked before init */
    if (unlikely(vb_opts == VB_UNDEF)) {
        options_parse_entry("--silent", &silent);
    }

    if (!silent) {
        vprint(stderr, "DLB WARNING", fmt, list);
    }
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
    /* Parse --quiet and --silent options if info() was invoked before init */
    if (unlikely(vb_opts == VB_UNDEF)) {
        options_parse_entry("--quiet", &quiet);
        options_parse_entry("--silent", &silent);
    }

    if (!quiet && !silent) {
        vprint(stderr, "DLB", fmt, list);
    }
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

void info0_force_print(const char *fmt, ...) {
#ifdef MPI_LIB
    if (_mpi_rank <= 0) {
#endif
        va_list list;
        va_start(list, fmt);
        vprint(stderr, "DLB", fmt, list);
        va_end(list);
#ifdef MPI_LIB
    }
#endif
}

#undef verbose
void verbose(verbose_opts_t flag, const char *fmt, ...) {
    /* Parse verbose options if verbose() was invoked before init */
    if (unlikely(vb_opts == VB_UNDEF)) {
        options_parse_entry("--verbose", &vb_opts);
    }

    if (quiet) return;

    va_list list;
    va_start(list, fmt);
    if      (vb_opts & flag & VB_API)     { vprint(stderr, "DLB API", fmt, list); }
    else if (vb_opts & flag & VB_MICROLB) { vprint(stderr, "DLB MICROLB", fmt, list); }
    else if (vb_opts & flag & VB_SHMEM)   { vprint(stderr, "DLB SHMEM", fmt, list); }
    else if (vb_opts & flag & VB_MPI_API) { vprint(stderr, "DLB MPI API", fmt, list); }
    else if (vb_opts & flag & VB_MPI_INT) { vprint(stderr, "DLB MPI INT", fmt, list); }
    else if (vb_opts & flag & VB_STATS)   { vprint(stderr, "DLB STATS", fmt, list); }
    else if (vb_opts & flag & VB_DROM)    { vprint(stderr, "DLB DROM", fmt, list); }
    else if (vb_opts & flag & VB_ASYNC)   { vprint(stderr, "DLB ASYNC", fmt, list); }
    else if (vb_opts & flag & VB_OMPT)    { vprint(stderr, "DLB OMPT", fmt, list); }
    else if (vb_opts & flag & VB_AFFINITY){ vprint(stderr, "DLB AFFINITY", fmt, list); }
    else if (vb_opts & flag & VB_BARRIER) { vprint(stderr, "DLB BARRIER", fmt, list); }
    else if (vb_opts & flag & VB_TALP)    { vprint(stderr, "DLB TALP", fmt, list); }
    else if (vb_opts & flag & VB_INSTR)   { vprint(stderr, "DLB INSTRUMENT", fmt, list); }
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

static void clean_shmems(pid_t id, const char *shmem_key) {
    if (shmem_cpuinfo__exists()) {
        shmem_cpuinfo__finalize(id, shmem_key);
    }
    if (shmem_procinfo__exists()) {
        shmem_procinfo__finalize(id, false, shmem_key);
    }
    if (shmem_talp__exists()) {
        shmem_talp__finalize(id);
    }
    shmem_async_finalize(id);
}

void dlb_clean(void) {
    pthread_mutex_lock(&dlb_clean_mutex);
    {
        /* First, try to finalize shmems of registered subprocess */
        const subprocess_descriptor_t** spds = spd_get_spds();
        const subprocess_descriptor_t** spd = spds;
        while (*spd) {
            pid_t id = (*spd)->id;
            const char *shmem_key = (*spd)->options.shm_key;
            clean_shmems(id, shmem_key);
            ++spd;
        }
        free(spds);

        /* Then, try to finalize current pid */
        pid_t pid = thread_spd ? thread_spd->id : getpid();
        const char *shmem_key = thread_spd ? thread_spd->options.shm_key : NULL;
        clean_shmems(pid, shmem_key);

        /* Finalize shared memories that do not support subprocesses */
        shmem_barrier__finalize(shmem_key);
        finalize_comm();

        /* Destroy shared memories if they still exist */
        const char *shmem_names[] = {"cpuinfo", "procinfo", "talp", "async"};
        enum { shmem_nelems = sizeof(shmem_names) / sizeof(shmem_names[0]) };
        int i;
        for (i=0; i<shmem_nelems; ++i) {
            if (shmem_exists(shmem_names[i], shmem_key)) {
                shmem_destroy(shmem_names[i], shmem_key);
            }
        }
    }
    pthread_mutex_unlock(&dlb_clean_mutex);
}

/* Trigger warning on some errors, tipically common or complex errors during init */
void warn_error(int error) {
    switch(error) {
        case DLB_ERR_NOMEM:
            warning("DLB could not be initialized due to insufficient space in the"
                    " shared memory. If you need to register a high amount of processes"
                    " or believe that this is a bug, please contact us at " PACKAGE_BUGREPORT);
            break;
        case DLB_ERR_PERM:
            if (thread_spd != NULL) {
                warning("The process with CPU affinity mask %s failed to initialize DLB."
                        " Please, check that each process initializing DLB has a"
                        " non-overlapping set of CPUs.", mu_to_str(&thread_spd->process_mask));
            } else {
                warning("This process has failed to initialize DLB."
                        " Please, check that each process initializing DLB has a"
                        " non-overlapping set of CPUs.");
            }
            if (shmem_procinfo__exists() && thread_spd != NULL) {
                warning("This is the list of current registered processes and their"
                        " affinity mask:");
                shmem_procinfo__print_info(thread_spd->options.shm_key);
            }
            break;
        case DLB_ERR_NOCOMP:
            warning("DLB could not initialize the shared memory due to incompatible"
                    " options among processes, likely ones sharing CPUs and others not."
                    " Please, if you believe this is a bug contact us at " PACKAGE_BUGREPORT);
            break;
    }
}


/* Print Buffers */

enum { INITIAL_BUFFER_SIZE = 1024 };

void printbuffer_init(print_buffer_t *buffer) {
    buffer->size = INITIAL_BUFFER_SIZE;
    buffer->addr = malloc(INITIAL_BUFFER_SIZE*sizeof(char));
    buffer->offset = buffer->addr;
    buffer->addr[0] = '\0';
}

void printbuffer_destroy(print_buffer_t *buffer) {
    free(buffer->addr);
    buffer->addr = NULL;
    buffer->offset = NULL;
    buffer->size = 0;
}

void printbuffer_append(print_buffer_t *buffer, const char *line) {
    /* Realloc buffer if needed */
    size_t line_len = strlen(line) + 2; /* + '\n\0' */
    size_t buffer_len = strlen(buffer->addr);
    if (buffer_len + line_len > buffer->size) {
        buffer->size *= 2;
        void *p = realloc(buffer->addr, buffer->size*sizeof(char));
        if (p) {
            buffer->addr = p;
            buffer->offset = buffer->addr + buffer_len;
        } else {
            fatal("realloc failed");
        }
    }

    /* Append line to buffer */
    buffer->offset += sprintf(buffer->offset, "%s\n", line);
    buffer_len = buffer->offset - buffer->addr;
    ensure(strlen(buffer->addr) == buffer_len, "buffer len is not correctly computed");
}
