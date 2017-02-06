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

#ifndef MYTIME_H
#define MYTIME_H

#include <time.h>       // for timespec: sec + nsec
#include <sys/time.h>   // for timeval: sec + usec
#include <inttypes.h>

void get_time( struct timespec *t );
void get_time_coarse( struct timespec *t );
int diff_time( struct timespec init, struct timespec end, struct timespec* diff );
void add_time( struct timespec t1, struct timespec t2, struct timespec* sum );
void mult_time( struct timespec t1, int factor, struct timespec* prod );
void reset( struct timespec *t1 );
double to_secs( struct timespec t1 );
int64_t timeval_diff( const struct timeval *init, const struct timeval *end );
int64_t timespec_diff( const struct timespec *start, const struct timespec *finish );
void add_tv_to_ts( const struct timeval *t1, const struct timeval *t2, struct timespec *res );
int64_t ts_to_ns( const struct timespec *ts );

#endif /* MYTIME_H */
