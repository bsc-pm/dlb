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

void verbose0 ( const char *fmt, ... );
void verbose ( const char *fmt, ... );
void fatal0 ( const char *fmt, ... );
void fatal ( const char *fmt, ... );
void debug_basic_info0 ( const char *fmt, ... );
void debug_basic_info ( const char *fmt, ... );
void debug_config ( const char *fmt, ... );
void debug_inout ( const char *fmt, ... );
void debug_inout_MPI ( const char *fmt, ... );
void debug_lend ( const char *fmt, ... );
void debug_shmem ( const char *fmt, ... );

#endif /* DEBUG_H */
