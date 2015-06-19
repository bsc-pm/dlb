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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <unistd.h>
#include <stdarg.h>
#include <execinfo.h>
#include "debug.h"
#include "globals.h"
#include "utils.h"
#include "LB_numThreads/numThreads.h"

#define VBFORMAT_LEN 32
static char fmt_str[VBFORMAT_LEN];
static verbose_opts_t vb_opts = VB_CLEAR;
static verbose_fmt_t vb_fmt = VB_CLEAR;

static void print_prefix( FILE *fp, const char *str ) {
    if ( vb_fmt & VBF_THREAD ) {
        fprintf( fp, "%s[%s:%d]: ", str, fmt_str, get_thread_num() );
    } else {
        fprintf( fp, "%s[%s]", str, fmt_str );
    }
}

void debug_init() {
    parse_env_verbose_opts( "LB_VERBOSE", &vb_opts );
    parse_env_verbose_format( "LB_VERBOSE_FORMAT", &vb_fmt, VBF_NODE|VBF_PID|VBF_THREAD );

    int i = 0;
    if ( vb_fmt & VBF_NODE ) {
        char hostname[HOST_NAME_MAX];
        gethostname( hostname, HOST_NAME_MAX );
        i += sprintf( &fmt_str[i], "%s:", hostname);
    }
    if ( vb_fmt & VBF_PID ) { i += sprintf( &fmt_str[i], "%d:", getpid()); }
    if ( vb_fmt & VBF_MPINODE ) { i += sprintf( &fmt_str[i], "%d:", _node_id); }
    if ( vb_fmt & VBF_MPIRANK ) { i += sprintf( &fmt_str[i], "%d:", _mpi_rank); }

    // Remove last separator ':' if fmt_str is not empty
    if ( i !=0 ) {
        fmt_str[i-1] = '\0';
    }
}

#if 0
void verbose ( const char *fmt, ... ) {
    va_list args;
    va_start( args, fmt );
    fprintf( stdout, "DLB[%s]: ", vb_format() );
    vfprintf( stdout, fmt, args );

    fprintf( stdout, "- " );
    if ( vb_opts & VB_API ) fprintf( stdout, "api " );
    if ( vb_opts & VB_MICROLB ) fprintf( stdout, "microlb " );
    if ( vb_opts & VB_SHMEM ) fprintf( stdout, "shmem " );
    if ( vb_opts & VB_MPI_API ) fprintf( stdout, "mpi_api " );
    if ( vb_opts & VB_MPI_INT ) fprintf( stdout, "mpi_int " );
    if ( vb_opts & VB_STATS ) fprintf( stdout, "stats " );
    if ( vb_opts & VB_DROM ) fprintf( stdout, "drom " );

    fprintf( stdout, "\n" );
    va_end( args );
}
#endif

void fatal0 ( const char *fmt, ... ) {
    if ( _mpi_rank <= 0 ) {
        va_list args;
        va_start( args, fmt );
        fprintf( stderr, "DLB PANIC[%d]: ", _process_id );
        vfprintf( stderr, fmt, args );
        va_end( args );
    }
    abort();
}

void fatal ( const char *fmt, ... ) {
    va_list args;
    va_start( args, fmt );
    fprintf( stderr, "DLB PANIC[%d]: ", _mpi_rank );
    vfprintf( stderr, fmt, args );
    va_end( args );
    abort();
}

void warning0 ( const char *fmt, ... ) {
    if ( _mpi_rank <= 0 ) {
        va_list args;
        va_start( args, fmt );
        fprintf( stderr, "DLB WARNING[%d]: ", _process_id );
        vfprintf( stderr, fmt, args );
        va_end( args );
    }
}

void warning ( const char *fmt, ... ) {
    va_list args;
    va_start( args, fmt );
    fprintf( stderr, "DLB WARNING[%d]: ", _mpi_rank );
    vfprintf( stderr, fmt, args );
    va_end( args );
}

void warningT ( const char *fmt, ... ) {
    va_list args;
    va_start( args, fmt );
    fprintf( stderr, "DLB WARNING[%d:%d]: ", _process_id, get_thread_num() );
    vfprintf( stderr, fmt, args );
    va_end( args );
}

#ifndef QUIET_MODE
void verbose0 ( const char *fmt, ... ) {
    if ( _mpi_rank <= 0 ) {
        va_list args;
        va_start( args, fmt );
        fprintf( stdout, "DLB[%d]: ", _process_id );
        vfprintf( stdout, fmt, args );
        va_end( args );
    }
}

void verbose ( const char *fmt, ... ) {
    FILE *fp = stdout;
    va_list args;
    print_prefix( fp, "DLB" );
    va_start( args, fmt );
    vfprintf( fp, fmt, args );
    va_end( args );
    fputc( '\n', fp );
}

void verboseT ( const char *fmt, ... ) {
    va_list args;
    va_start( args, fmt );
    fprintf( stdout, "DLB[%d:%d]: ", _process_id, get_thread_num() );
    vfprintf( stdout, fmt, args );
    va_end( args );
}
#endif

#ifdef debugBasicInfo
void debug_basic_info0 ( const char *fmt, ... ) {
    if ( _mpi_rank <= 0 ) {
        va_list args;
        va_start( args, fmt );
        fprintf( stdout, "DLB[%d]: ", _process_id );
        vfprintf( stdout, fmt, args );
        va_end( args );
    }
}

void debug_basic_info ( const char *fmt, ... ) {
    va_list args;
    va_start( args, fmt );
    fprintf( stdout, "DLB[%d:%d]: ", _node_id, _process_id );
    vfprintf( stdout, fmt, args );
    va_end( args );
}
#endif

#ifdef debugConfig
void debug_config ( const char *fmt, ... ) {
    if ( !_verbose ) { return; }

    va_list args;
    va_start( args, fmt );
    fprintf( stderr, "DLB[%d:%d]: ", _node_id, _process_id );
    vfprintf( stderr, fmt, args );
    va_end( args );
}
#endif

#ifdef debugInOut
void debug_inout ( const char *fmt, ... ) {
    if ( !_verbose ) { return; }

    va_list args;
    va_start( args, fmt );
    fprintf( stderr, "DLB[%d:%d]: ", _node_id, _process_id );
    vfprintf( stderr, fmt, args );
    va_end( args );
}
#endif

#ifdef debugInOutMPI
void debug_inout_MPI ( const char *fmt, ... ) {
    if ( !_verbose ) { return; }

    va_list args;
    va_start( args, fmt );
    vfprintf( stderr, fmt, args );
    va_end( args );
}
#endif

#ifdef debugBlockingMPI
void debug_blocking_MPI ( const char *fmt, ... ) {
    if ( !_verbose ) { return; }

    va_list args;
    va_start( args, fmt );
    fprintf( stderr, "DLB[%d:%d]: ", _node_id, _process_id );
    vfprintf( stderr, fmt, args );
    va_end( args );
}
#endif

#ifdef debugLend
void debug_lend ( const char *fmt, ... ) {
    if ( !_verbose ) { return; }

    va_list args;
    va_start( args, fmt );
    fprintf( stderr, "DLB[%d:%d]: ", _node_id, _process_id );
    vfprintf( stderr, fmt, args );
    va_end( args );
}
#endif

#ifdef debugSharedMem
void debug_shmem ( const char *fmt, ... ) {
    if ( !_verbose ) { return; }

    va_list args;
    va_start( args, fmt );
    fprintf( stderr, "DLB[%d:%d]: ", _node_id, _process_id );
    vfprintf( stderr, fmt, args );
    va_end( args );
}
#endif

void print_backtrace ( void )
{
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
