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

#ifndef DEBUG_H
#define DEBUG_H

void fatal0 ( const char *fmt, ... );
void fatal ( const char *fmt, ... );
void warning0 ( const char *fmt, ... );
void warning ( const char *fmt, ... );

#ifndef QUIET_MODE
void verbose0 ( const char *fmt, ... );
void verbose ( const char *fmt, ... );
#else
#define verbose0(fmt, ...)
#define verbose(fmt, ...)
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

#ifdef ENABLE_DEBUG
#define DLB_DEBUG(f) f
#else
#define DLB_DEBUG(f)
#endif

#endif /* DEBUG_H */
