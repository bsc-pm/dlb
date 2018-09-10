/*********************************************************************************/
/*  Copyright 2009-2018 Barcelona Supercomputing Center                          */
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

/*<testinfo>
    test_generator="gens/basic-generator"
</testinfo>*/

#include "support/mytime.h"

#include <unistd.h>
#include <assert.h>

int main(int argc, char **argv) {

    struct timeval tv0, tv1;
    struct timespec result, time0, time1, time2;
    reset(&time0);
    get_time_coarse(&time1);
    get_time(&time2);


    /* diff_time */
    time0 = (struct timespec){.tv_sec=1, .tv_nsec=1};
    time1 = (struct timespec){.tv_sec=0, .tv_nsec=0};
    assert( diff_time(time0, time1, &result) == -1 );

    time0 = (struct timespec){.tv_sec=1, .tv_nsec=1};
    time1 = (struct timespec){.tv_sec=0, .tv_nsec=0};
    assert( diff_time(time0, time1, &result) == -1 );

    time0 = (struct timespec){.tv_sec=1, .tv_nsec=1};
    time1 = (struct timespec){.tv_sec=1, .tv_nsec=0};
    assert( diff_time(time0, time1, &result) == -1 );

    time0 = (struct timespec){.tv_sec=1, .tv_nsec=0};
    time1 = (struct timespec){.tv_sec=1, .tv_nsec=0};
    assert( diff_time(time0, time1, &result) == 0 );
    assert( result.tv_sec == 0 && result.tv_nsec == 0 );
    assert( to_secs(result) > -0.1 && to_secs(result) < 0.1 );
    assert( to_nsecs(&result) == 0 );

    time0 = (struct timespec){.tv_sec=1, .tv_nsec=0};
    time1 = (struct timespec){.tv_sec=1, .tv_nsec=1};
    assert( diff_time(time0, time1, &result) == 0 );
    assert( result.tv_sec == 0 && result.tv_nsec == 1 );
    assert( to_secs(result) > 0.0 && to_secs(result) < 0.2 );
    assert( to_nsecs(&result) == 1 );

    time0 = (struct timespec){.tv_sec=42, .tv_nsec=0};
    time1 = (struct timespec){.tv_sec=52, .tv_nsec=0};
    assert( diff_time(time0, time1, &result) == 0 );
    assert( result.tv_sec == 10 && result.tv_nsec == 0 );
    assert( to_secs(result) > 9.9 && to_secs(result) < 10.1 );
    assert( to_nsecs(&result) == 10000000000LL );

    time0 = (struct timespec){.tv_sec=42, .tv_nsec=500};
    time1 = (struct timespec){.tv_sec=52, .tv_nsec=500};
    assert( diff_time(time0, time1, &result) == 0 );
    assert( result.tv_sec == 10 && result.tv_nsec == 0 );
    assert( to_secs(result) > 9.9 && to_secs(result) < 10.1 );
    assert( to_nsecs(&result) == 10000000000LL );

    time0 = (struct timespec){.tv_sec=42, .tv_nsec=5000};
    time1 = (struct timespec){.tv_sec=52, .tv_nsec=500};
    assert( diff_time(time0, time1, &result) == 0 );
    assert( result.tv_sec == 9 && result.tv_nsec == 999995500L );
    assert( to_secs(result) > 9.9 && to_secs(result) < 10.1 );
    assert( to_nsecs(&result) == 9999995500LL );

    time0 = (struct timespec){.tv_sec=50042, .tv_nsec=5000};
    time1 = (struct timespec){.tv_sec=50052, .tv_nsec=500};
    assert( diff_time(time0, time1, &result) == 0 );
    assert( to_secs(result) > 9.9 && to_secs(result) < 10.1 );
    assert( to_nsecs(&result) == 9999995500LL );
    assert( result.tv_sec == 9 && result.tv_nsec == 999995500L );

    /* add_time */
    time0 = (struct timespec){.tv_sec=0, .tv_nsec=0};
    add_time(time0, time0, &result);
    assert( result.tv_sec == 0 && result.tv_nsec == 0 );

    time0 = (struct timespec){.tv_sec=0, .tv_nsec=500000000};
    add_time(time0, time0, &result);
    assert( result.tv_sec == 1 && result.tv_nsec == 0 );

    /* mult_time */
    time0 = (struct timespec){.tv_sec=0, .tv_nsec=0};
    mult_time(time0, 2, &result);
    assert( result.tv_sec == 0 && result.tv_nsec == 0 );

    time0 = (struct timespec){.tv_sec=5, .tv_nsec=555555555};
    mult_time(time0, 5, &result);
    assert( result.tv_sec == 27 && result.tv_nsec == 777777775 );

    /* timeval_diff */
    tv0 = (struct timeval){.tv_sec=0, .tv_usec=0};
    assert( timeval_diff(&tv0, &tv0) == 0 );

    tv0 = (struct timeval){.tv_sec=0, .tv_usec=500};
    tv1 = (struct timeval){.tv_sec=5, .tv_usec=50};
    assert( timeval_diff(&tv0, &tv1) == 4999550 );

    /* add_tv_to_ts */
    tv0 = (struct timeval){.tv_sec=0, .tv_usec=10};
    add_tv_to_ts(&tv0, &tv0, &result);
    assert( result.tv_sec == 0 && result.tv_nsec == 20000 );

    tv0 = (struct timeval){.tv_sec=1, .tv_usec=999999};
    add_tv_to_ts(&tv0, &tv0, &result);
    assert( result.tv_sec == 3 && result.tv_nsec == 999998000 );

    return 0;
}
