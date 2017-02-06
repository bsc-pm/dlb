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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <sys/types.h>
#include <sys/syscall.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <stdarg.h>
#include <execinfo.h>

#include "support/debug.h"
#include "support/options.h"

#define VBFORMAT_LEN 32
verbose_opts_t vb_opts;
static verbose_fmt_t vb_fmt;
static char fmt_str[VBFORMAT_LEN];

void debug_init(const options_t *options) {
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

void vb_print(FILE *fp, const char *prefix, const char *fmt, ...) {
    // Print prefix and object identifier
    if ( vb_fmt & VBF_THREAD ) {
        fprintf( fp, "%s[%s:%ld]: ", prefix, fmt_str, syscall(SYS_gettid) );
    } else {
        fprintf( fp, "%s[%s]: ", prefix, fmt_str );
    }

    // Print va_list
    va_list args;
    va_start( args, fmt );
    vfprintf( fp, fmt, args );
    va_end( args );
    fputc( '\n', fp );
}

void print_backtrace(void) {
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

}
