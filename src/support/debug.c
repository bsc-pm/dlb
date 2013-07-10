/*************************************************************************************/
/*      Copyright 2012 Barcelona Supercomputing Center                               */
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

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <mpi.h>
#include "globals.h"

void fatal0 ( const char *fmt, ... )
{
   if ( _mpi_rank == 0 ) {
      va_list args;
      va_start( args, fmt );
      fprintf( stderr, "DLB PANIC[*]: " );
      vfprintf( stderr, fmt, args );
      va_end( args );
   }
   PMPI_Abort ( MPI_COMM_WORLD, 1 );
}

void fatal ( const char *fmt, ... )
{
   va_list args;
   va_start( args, fmt );
   fprintf( stderr, "DLB PANIC[%d]: ", _mpi_rank );
   vfprintf( stderr, fmt, args );
   va_end( args );
   PMPI_Abort ( MPI_COMM_WORLD, 1 );
}

void warning0 ( const char *fmt, ... )
{
   if ( _mpi_rank == 0 ) {
      va_list args;
      va_start( args, fmt );
      fprintf( stderr, "DLB WARNING[*]: " );
      vfprintf( stderr, fmt, args );
      va_end( args );
   }
}

void warning ( const char *fmt, ... )
{
   va_list args;
   va_start( args, fmt );
   fprintf( stderr, "DLB WARNING[%d]: ", _mpi_rank );
   vfprintf( stderr, fmt, args );
   va_end( args );
}

#ifndef QUIET_MODE
void verbose0 ( const char *fmt, ... )
{
   if ( _mpi_rank == 0 ) {
      va_list args;
      va_start( args, fmt );
      fprintf( stdout, "DLB[*]: " );
      vfprintf( stdout, fmt, args );
      va_end( args );
   }
}

void verbose ( const char *fmt, ... )
{
   va_list args;
   va_start( args, fmt );
   fprintf( stdout, "DLB[%d]: ", _mpi_rank );
   vfprintf( stdout, fmt, args );
   va_end( args );
}
#endif

#ifdef debugBasicInfo
void debug_basic_info0 ( const char *fmt, ... )
{
   if ( _mpi_rank == 0 ) {
      va_list args;
      va_start( args, fmt );
      fprintf( stdout, "DLB[*]: " );
      vfprintf( stdout, fmt, args );
      va_end( args );
   }
}

void debug_basic_info ( const char *fmt, ... )
{
   va_list args;
   va_start( args, fmt );
   fprintf( stdout, "DLB[%d:%d]: ", _node_id, _process_id );
   vfprintf( stdout, fmt, args );
   va_end( args );
}
#endif

#ifdef debugConfig
void debug_config ( const char *fmt, ... )
{
   va_list args;
   va_start( args, fmt );
   fprintf( stderr, "DLB[%d:%d]: ", _node_id, _process_id );
   vfprintf( stderr, fmt, args );
   va_end( args );
}
#endif

#ifdef debugInOut
void debug_inout ( const char *fmt, ... )
{
   va_list args;
   va_start( args, fmt );
   fprintf( stderr, "DLB[%d:%d]: ", _node_id, _process_id );
   vfprintf( stderr, fmt, args );
   va_end( args );
}
#endif

#ifdef debugInOutMPI
void debug_inout_MPI ( const char *fmt, ... )
{
   va_list args;
   va_start( args, fmt );
   vfprintf( stderr, fmt, args );
   va_end( args );
}
#endif

#ifdef debugBlockingMPI
void debug_blocking_MPI ( const char *fmt, ... )
{
   va_list args;
   va_start( args, fmt );
   fprintf( stderr, "DLB[%d:%d]: ", _node_id, _process_id );
   vfprintf( stderr, fmt, args );
   va_end( args );
}
#endif

#ifdef debugLend
void debug_lend ( const char *fmt, ... )
{
   va_list args;
   va_start( args, fmt );
   fprintf( stderr, "DLB[%d:%d]: ", _node_id, _process_id );
   vfprintf( stderr, fmt, args );
   va_end( args );
}
#endif

#ifdef debugSharedMem
void debug_shmem ( const char *fmt, ... )
{
   va_list args;
   va_start( args, fmt );
   fprintf( stderr, "DLB[%d:%d]: ", _node_id, _process_id );
   vfprintf( stderr, fmt, args );
   va_end( args );
}
#endif
