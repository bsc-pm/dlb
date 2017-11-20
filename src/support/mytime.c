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

#include "support/mytime.h"

enum { MS_PER_SECOND = 1000LL };
enum { US_PER_SECOND = 1000000LL };
enum { NS_PER_SECOND = 1000000000LL };

void get_time( struct timespec *t ) {
    clock_gettime( CLOCK_MONOTONIC, t);
}
void get_time_coarse( struct timespec *t ) {
#ifdef CLOCK_MONOTONIC_COARSE
    clock_gettime( CLOCK_MONOTONIC_COARSE, t);
#else
    clock_gettime( CLOCK_MONOTONIC, t);
#endif
}

int diff_time( struct timespec init, struct timespec end, struct timespec* diff ) {
    if ( init.tv_sec > end.tv_sec ) {
        return -1;
    } else {
        if ( ( init.tv_sec == end.tv_sec ) && ( init.tv_nsec > end.tv_nsec ) ) {
            return -1;
        } else {
            if ( init.tv_nsec > end.tv_nsec ) {
                diff->tv_sec = end.tv_sec - ( init.tv_sec + 1 );
                diff->tv_nsec = ( end.tv_nsec + NS_PER_SECOND ) - init.tv_nsec;
            } else {
                diff->tv_sec = end.tv_sec - init.tv_sec;
                diff->tv_nsec = end.tv_nsec - init.tv_nsec;
            }
        }
    }

    return 0;
}

void add_time( struct timespec t1, struct timespec t2, struct timespec* sum ) {
    sum->tv_nsec = t1.tv_nsec + t2.tv_nsec;

    if ( sum->tv_nsec >= NS_PER_SECOND ) {
        sum->tv_sec = t1.tv_sec + t2.tv_sec + ( sum->tv_nsec/NS_PER_SECOND );
        sum->tv_nsec = ( sum->tv_nsec % NS_PER_SECOND );
    } else {
        sum->tv_sec = t1.tv_sec + t2.tv_sec;
    }
}

void mult_time( struct timespec t1, int factor, struct timespec* prod ) {
    int64_t nsec = (int64_t)t1.tv_nsec * factor;
    if (nsec >= NS_PER_SECOND) {
        prod->tv_sec = t1.tv_sec * factor + nsec / NS_PER_SECOND;
        prod->tv_nsec = nsec % NS_PER_SECOND;
    } else {
        prod->tv_nsec = nsec;
        prod->tv_sec = t1.tv_sec * factor;
    }
}

void reset( struct timespec *t1 ) {
    t1->tv_nsec = 0;
    t1->tv_sec = 0;
}

double to_secs( struct timespec t1 ) {
    return t1.tv_sec + (double)t1.tv_nsec / NS_PER_SECOND;
}

int64_t to_nsecs( const struct timespec *ts ) {
    return ts->tv_nsec + (int64_t)ts->tv_sec * NS_PER_SECOND;
}

// Return timeval diff in us
int64_t timeval_diff( const struct timeval *init, const struct timeval *end ) {
    return (int64_t)(end->tv_sec - init->tv_sec) * US_PER_SECOND +
        (int64_t)(end->tv_usec - init->tv_usec);
}

// Return timespec diff in ns
int64_t timespec_diff( const struct timespec *init, const struct timespec *end ) {
    return (int64_t)(end->tv_sec - init->tv_sec) * NS_PER_SECOND +
        (int64_t)(end->tv_nsec - init->tv_nsec);
}

void add_tv_to_ts( const struct timeval *t1, const struct timeval *t2,
                    struct timespec *res ) {
    long sec = t2->tv_sec + t1->tv_sec;
    long nsec = (t2->tv_usec + t1->tv_usec) * MS_PER_SECOND;
    if (nsec >= NS_PER_SECOND) {
        nsec -= NS_PER_SECOND;
        sec++;
    }
    res->tv_sec = sec;
    res->tv_nsec = nsec;
}
