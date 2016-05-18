/*********************************************************************************/
/*  Copyright 2015 Barcelona Supercomputing Center                               */
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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <sched.h>
#include <pthread.h>
#include "LB_numThreads/numThreads.h"
#include "support/globals.h"
#include "support/tracing.h"
#include "support/debug.h"
#include "support/mask_utils.h"

#define NANOS_SYMBOLS_DEFINED ( \
        nanos_omp_get_process_mask && \
        nanos_omp_set_process_mask && \
        nanos_omp_add_process_mask && \
        nanos_omp_get_active_mask && \
        nanos_omp_set_process_mask && \
        nanos_omp_add_process_mask && \
        nanos_omp_get_thread_num && \
        nanos_omp_get_max_threads && \
        nanos_omp_set_num_threads \
        )

#define OMP_SYMBOLS_DEFINED ( \
        omp_get_thread_num && \
        omp_get_max_threads && \
        omp_set_num_threads \
        )

/* Weak symbols */
void nanos_omp_get_process_mask(cpu_set_t *cpu_set) __attribute__((weak));
int  nanos_omp_set_process_mask(const cpu_set_t *cpu_set) __attribute__ ((weak));
void nanos_omp_add_process_mask(const cpu_set_t *cpu_set) __attribute__ ((weak));
void nanos_omp_get_active_mask(cpu_set_t *cpu_set) __attribute__ ((weak));
int  nanos_omp_set_active_mask(const cpu_set_t *cpu_set) __attribute__((weak));
void nanos_omp_add_active_mask(const cpu_set_t *cpu_set) __attribute__((weak));
int  nanos_omp_get_thread_num(void) __attribute__((weak));
int  nanos_omp_get_max_threads(void) __attribute__((weak));
void nanos_omp_set_num_threads(int nthreads) __attribute__((weak));
int  omp_get_thread_num(void) __attribute__((weak));
int  omp_get_max_threads(void) __attribute__((weak));
void omp_set_num_threads(int nthreads) __attribute__((weak));


// Static functions to be called when no Prog Model is found
static void unknown_get_process_mask(cpu_set_t *cpu_set) {
    sched_getaffinity(0, sizeof(cpu_set), cpu_set);
}
static int  unknown_set_process_mask(const cpu_set_t *cpu_set) { return 0; }
static void unknown_add_process_mask(const cpu_set_t *cpu_set) {}
static void unknown_get_active_mask(cpu_set_t *cpu_set) {
    return unknown_get_process_mask(cpu_set);
}
static int  unknown_set_active_mask(const cpu_set_t *cpu_set) { return 0; }
static void unknown_add_active_mask(const cpu_set_t *cpu_set) {}
static int  unknown_get_thread_num(void) { return 0; }
static int  unknown_get_threads(void) { return 1;}
static void unknown_set_threads(int nthreads) {}


static struct {
    void (*get_process_mask) (cpu_set_t *cpu_set);
    int  (*set_process_mask) (const cpu_set_t *cpu_set);
    void (*add_process_mask) (const cpu_set_t *cpu_set);
    void (*get_active_mask) (cpu_set_t *cpu_set);
    int  (*set_active_mask) (const cpu_set_t *cpu_set);
    void (*add_active_mask) (const cpu_set_t *cpu_set);
    int  (*get_thread_num) (void);
    int  (*get_threads) (void);
    void (*set_threads) (int nthreads);
} pm_funcs = {
    unknown_get_process_mask,
    unknown_set_process_mask,
    unknown_add_process_mask,
    unknown_get_active_mask,
    unknown_set_active_mask,
    unknown_add_active_mask,
    unknown_get_thread_num,
    unknown_get_threads,
    unknown_set_threads
};

static int cpus_node;

void pm_init( void ) {
    cpus_node = mu_get_system_size();

    /* Nanos++ */
    if ( NANOS_SYMBOLS_DEFINED ) {
        pm_funcs.get_process_mask = nanos_omp_get_process_mask;
        pm_funcs.set_process_mask = nanos_omp_set_process_mask;
        pm_funcs.add_process_mask = nanos_omp_add_process_mask;
        pm_funcs.get_active_mask = nanos_omp_get_active_mask;
        pm_funcs.set_active_mask = nanos_omp_set_active_mask;
        pm_funcs.add_active_mask = nanos_omp_add_active_mask;
        pm_funcs.get_thread_num = nanos_omp_get_thread_num;
        pm_funcs.get_threads = nanos_omp_get_max_threads;
        pm_funcs.set_threads = nanos_omp_set_num_threads;
    }
    /* OpenMP */
    else if ( OMP_SYMBOLS_DEFINED ) {
        pm_funcs.get_thread_num = omp_get_thread_num;
        pm_funcs.get_threads = omp_get_max_threads;
        pm_funcs.set_threads = omp_set_num_threads;
    }
    /* Undefined */
    else {
        pm_funcs.get_process_mask = unknown_get_process_mask;
        pm_funcs.set_process_mask = unknown_set_process_mask;
        pm_funcs.add_process_mask = unknown_add_process_mask;
        pm_funcs.get_active_mask = unknown_get_active_mask;
        pm_funcs.set_active_mask = unknown_set_active_mask;
        pm_funcs.add_active_mask = unknown_add_active_mask;
        pm_funcs.get_thread_num = unknown_get_thread_num;
        pm_funcs.get_threads = unknown_get_threads;
        pm_funcs.set_threads = unknown_set_threads;
    }
    _default_nthreads = pm_funcs.get_threads();
}

void update_threads(int threads) {
    if ( threads > cpus_node ) {
        warning( "Trying to use more CPUS (%d) than available (%d)\n", threads, cpus_node);
        threads = cpus_node;
    }

    add_event(THREADS_USED_EVENT, threads);

    threads = (threads<1) ? 1 : threads;
    pm_funcs.set_threads( threads );
}

void get_mask( cpu_set_t *cpu_set ) {
    pm_funcs.get_active_mask( cpu_set );
}

int  set_mask( const cpu_set_t *cpu_set ) {
    return pm_funcs.set_active_mask( cpu_set );
}

void add_mask( const cpu_set_t *cpu_set ) {
    pm_funcs.add_active_mask( cpu_set );
}

void get_process_mask( cpu_set_t *cpu_set ) {
    pm_funcs.get_process_mask( cpu_set );
}

int  set_process_mask( const cpu_set_t *cpu_set ) {
    return pm_funcs.set_process_mask( cpu_set );
}

void add_process_mask( const cpu_set_t *cpu_set ) {
    pm_funcs.add_process_mask( cpu_set );
}

int get_thread_num( void ) {
    return pm_funcs.get_thread_num();
}
