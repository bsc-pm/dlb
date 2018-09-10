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

#include "support/mytime.h"

#include "support/debug.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

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

int64_t get_time_in_ns(void) {
    struct timespec t;
    get_time(&t);
    return to_nsecs(&t);
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

/* Timers */

enum { TIMER_MAX_KEY_LEN = 128 };

typedef struct TimerData {
    char key[TIMER_MAX_KEY_LEN];
    int64_t acc;
    int64_t asq;
    int64_t max;
    int64_t count;
    struct timespec start;
    struct timespec stop;
} timer_data_t;

static timer_data_t *timers = NULL;;
static size_t ntimers = 0;

void timer_init(void) {
}

void *timer_register(const char *key) {
    /* Found key if already registered */
    int i;
    for (i=0; i<ntimers; ++i) {
        if (strncmp(timers[i].key, key, TIMER_MAX_KEY_LEN) == 0) {
            return &timers[i];
        }
    }

    /* Reallocate new position in timers array */
    ++ntimers;
    void *p = realloc(timers, sizeof(timer_data_t)*ntimers);
    if (p) timers = p;
    else fatal("realloc failed");

    /* Initialize timer */
    timer_data_t *timer = &timers[ntimers-1];
    strncpy(&timer->key[0], key, TIMER_MAX_KEY_LEN);
    timer->acc = 0;
    timer->asq = 0;
    timer->max = 0;
    timer->count = 0;
    reset(&timer->start);
    reset(&timer->stop);

    return timer;
}

void timer_start(void *handler) {
    timer_data_t *timer = (timer_data_t*) handler;
    timer->count++;
    get_time(&timer->start);
}

void timer_stop(void *handler) {
    timer_data_t *timer = (timer_data_t*) handler;
    get_time(&timer->stop);
    int64_t elapsed = timespec_diff(&timer->start, &timer->stop);
    timer->acc += elapsed;
    timer->asq += elapsed*elapsed;
    timer->max = elapsed > timer->max ? elapsed : timer->max;
}

void timer_finalize(void) {
    /* Timers Report */
    int i;
    for (i=0; i<ntimers; ++i) {
        timer_data_t *timer = &timers[i];
        int64_t avg = timer->acc/timer->count;
        int64_t stdev = sqrt(timer->asq/timer->count - avg*avg);
        info("Timer \"%s\" "
                "n: %"PRId64", avg: %"PRId64", stdev: %"PRId64", max: %"PRId64,
                timer->key, timer->count, avg, stdev, timer->max);
    }

    /* De-allocate timers */
    free(timers);
    timers = NULL;
    ntimers = 0;
}
