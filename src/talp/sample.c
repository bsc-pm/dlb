/*********************************************************************************/
/*  Copyright 2009-2026 Barcelona Supercomputing Center                          */
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

#include "talp/sample.h"

#include "support/debug.h"
#include "support/dlb_common.h"
#include "support/mytime.h"
#include "support/tracing.h"
#include "talp/backend.h"
#include "talp/talp_hwc.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>


extern __thread bool thread_is_observer;

static __thread talp_sample_t* _tls_sample = NULL;
static __thread bool _is_main_sample = false;
static __thread bool _is_main_sample_in_serial_mode = false;

static void set_state(const talp_info_t *talp_info,
        talp_sample_t *sample, talp_sample_state_t new_state);


/*********************************************************************************/
/*    Init / Finalize                                                            */
/*********************************************************************************/

void talp_sample_init(talp_info_t *talp_info) {

    talp_info->sample_registry = (sample_registry_t){
        .mutex = PTHREAD_MUTEX_INITIALIZER,
    };
}

void talp_sample_finalize(talp_info_t *talp_info) {

    /* Warning about _tls_sample in worker threads:
     * worker threads do not call this function, so currently they are
     * not deallocating their sample.
     * In some cases, it might happen that a worker thread exits without
     * the main thread reducing its sample, so in these cases the sample
     * needs to outlive the thread.
     * The main thread could deallocate it at this point, but then the
     * TLS variable would be broken if TALP is reinitialized again.
     * For now we will keep it like this and will revisit if needed. */

    /* Deallocate main thread sample */
    free(_tls_sample);
    _tls_sample = NULL;

    /* Deallocate samples list */
    sample_registry_t *registry = &talp_info->sample_registry;
    pthread_mutex_lock(&registry->mutex);
    {
        free(registry->samples);
        registry->samples = NULL;
        registry->num_samples = 0;
    }
    pthread_mutex_unlock(&registry->mutex);
}


/*********************************************************************************/
/*    Sample getters & setters                                                   */
/*********************************************************************************/

bool talp_sample_is_main(void) {
    return _is_main_sample;
}

/* Quick test, without locking and without generating a new sample */
bool talp_sample_is_mine(const talp_sample_t *sample) {
    return sample != NULL && sample == _tls_sample;
}

/* Sets the TLS variable _is_main_sample_in_serial_mode. This function is
 * called by the main thread when beginning or ending parallel region of level 1.
 * FIXME: free agent threads may break this condition.
 *
 * Sets whether the main thread is running in serial mode. */
void talp_sample_set_main_serial_mode(bool serial_mode) {

    if (_is_main_sample) {
        _is_main_sample_in_serial_mode = serial_mode;
    }
}

/* Get the TLS associated sample */
talp_sample_t* talp_sample_get(talp_info_t *talp_info) {

    /* Thread already has an allocated sample, return it */
    if (likely(_tls_sample != NULL)) return _tls_sample;

    /* Observer threads don't have a valid sample */
    if (unlikely(thread_is_observer)) return NULL;

    /* Otherwise, allocate */
    sample_registry_t *registry = &talp_info->sample_registry;
    pthread_mutex_lock(&registry->mutex);
    {
        int num_samples = ++registry->num_samples;
        void *samples = realloc(registry->samples, sizeof(talp_sample_t*)*num_samples);
        if (samples) {
            void *new_sample = NULL;
            if (posix_memalign(&new_sample, DLB_CACHE_LINE, sizeof(talp_sample_t)) == 0) {
                _tls_sample = new_sample;
                *_tls_sample = (talp_sample_t){0};
                registry->samples = samples;
                registry->samples[num_samples-1] = new_sample;
                if (num_samples == 1) {
                    _is_main_sample = true;
                    _is_main_sample_in_serial_mode = true;
                }
            } else {
                // error
                free(new_sample);
                _tls_sample = NULL;
            }
        }
    }
    pthread_mutex_unlock(&registry->mutex);

    fatal_cond(_tls_sample == NULL, "TALP: could not allocate thread sample");

    /* If a thread is created mid-region, its initial time is that of the
     * innermost open region, otherwise it is the current time */
    int64_t last_updated_ts;
    if (talp_info->open_regions) {
        const dlb_monitor_t *monitor = talp_info->open_regions->data;
        last_updated_ts = monitor->start_time;
    } else {
        last_updated_ts = get_time_in_ns();
    }

    _tls_sample->last_updated_ts = last_updated_ts;

    set_state(talp_info, _tls_sample, TALP_STATE_DISABLED);

#ifdef INSTRUMENTATION_VERSION
    unsigned events[] = {MONITOR_CYCLES, MONITOR_INSTR};
    long long hwc_values[] = {0, 0};
    instrument_nevent(2, events, hwc_values);
#endif

    return _tls_sample;
}


/*********************************************************************************/
/*    Sample update                                                              */
/*********************************************************************************/

/* Compute new microsample (time since last update) and update sample values */
void talp_sample_update(talp_info_t *talp_info) {

    talp_sample_t *sample = talp_sample_get(talp_info);

    /* Observer threads ignore this function */
    if (unlikely(sample == NULL)) return;

    /* Compute duration and set new last_updated_ts */
    int64_t now = get_time_in_ns();
    int64_t microsample_duration = now - sample->last_updated_ts;
    sample->last_updated_ts = now;

    /* Update the appropriate sample timer */
    switch(sample->state) {
        case TALP_STATE_DISABLED:
            break;
        case TALP_STATE_USEFUL:
            DLB_ATOMIC_ADD_RLX(&sample->timers.useful, microsample_duration);
            break;
        case TALP_STATE_NOT_USEFUL_MPI:
            DLB_ATOMIC_ADD_RLX(&sample->timers.not_useful_mpi, microsample_duration);
            if (_is_main_sample_in_serial_mode) {
                // Add worker threads' time to special timer
                int num_cpus = talp_info->sample_registry.num_samples;
                DLB_ATOMIC_ADD_RLX(&sample->timers.not_useful_omp_during_mpi,
                        microsample_duration * (num_cpus-1));
            }
            break;
        case TALP_STATE_NOT_USEFUL_OMP_IN:
            DLB_ATOMIC_ADD_RLX(&sample->timers.not_useful_omp_in, microsample_duration);
            break;
        case TALP_STATE_NOT_USEFUL_OMP_OUT:
            DLB_ATOMIC_ADD_RLX(&sample->timers.not_useful_omp_out, microsample_duration);
            break;
        case TALP_STATE_NOT_USEFUL_GPU:
            DLB_ATOMIC_ADD_RLX(&sample->timers.not_useful_gpu, microsample_duration);
            break;
    }

    if (talp_info->flags.have_hwc) {
        hwc_measurements_t measurements;
        if (talp_hwc_collect(&measurements)) {
            /* Atomically add HWC values to sample structure */
            DLB_ATOMIC_ADD_RLX(&sample->counters.cycles, measurements.cycles);
            DLB_ATOMIC_ADD_RLX(&sample->counters.instructions, measurements.instructions);
        }

#ifdef INSTRUMENTATION_VERSION
        // It's safe to emit even if talp_hwc_collect returned false,
        // struct is zero'ed in that case
        unsigned events[] = {MONITOR_CYCLES, MONITOR_INSTR};
        long long hwc_values[] = {measurements.cycles, measurements.instructions};
        instrument_nevent(2, events, hwc_values);
#endif
    }
}

void talp_sample_update_foreign(talp_info_t *talp_info, talp_sample_t *sample, int64_t now) {

    /* Observer threads ignore this function */
    if (unlikely(sample == NULL)) return;

    /* Compute duration and set new last_updated_ts */
    int64_t microsample_duration = now - sample->last_updated_ts;
    sample->last_updated_ts = now;

    /* Update the appropriate sample timer */
    switch(sample->state) {
        case TALP_STATE_DISABLED:
            break;
        case TALP_STATE_USEFUL:
            DLB_ATOMIC_ADD_RLX(&sample->timers.useful, microsample_duration);
            break;
        case TALP_STATE_NOT_USEFUL_MPI:
            DLB_ATOMIC_ADD_RLX(&sample->timers.not_useful_mpi, microsample_duration);
            break;
        case TALP_STATE_NOT_USEFUL_OMP_IN:
            DLB_ATOMIC_ADD_RLX(&sample->timers.not_useful_omp_in, microsample_duration);
            break;
        case TALP_STATE_NOT_USEFUL_OMP_OUT:
            DLB_ATOMIC_ADD_RLX(&sample->timers.not_useful_omp_out, microsample_duration);
            break;
        case TALP_STATE_NOT_USEFUL_GPU:
            DLB_ATOMIC_ADD_RLX(&sample->timers.not_useful_gpu, microsample_duration);
            break;
    }
}


static void set_state(const talp_info_t *restrict talp_info,
        talp_sample_t *restrict sample, talp_sample_state_t new_state) {

    if (talp_info->flags.have_hwc) {
        talp_sample_state_t old = sample->state;
        talp_hwc_on_state_change(old, new_state);
    }

    sample->state = new_state;

    instrument_event(MONITOR_STATE,
            new_state == TALP_STATE_DISABLED ? MONITOR_STATE_DISABLED
            : new_state == TALP_STATE_USEFUL ? MONITOR_STATE_USEFUL
            : new_state == TALP_STATE_NOT_USEFUL_MPI ? MONITOR_STATE_NOT_USEFUL_MPI
            : new_state == TALP_STATE_NOT_USEFUL_OMP_IN ? MONITOR_STATE_NOT_USEFUL_OMP_IN
            : new_state == TALP_STATE_NOT_USEFUL_OMP_OUT ? MONITOR_STATE_NOT_USEFUL_OMP_OUT
            : new_state == TALP_STATE_NOT_USEFUL_GPU ? MONITOR_STATE_NOT_USEFUL_GPU
            : 0,
            EVENT_BEGIN);
}

void talp_sample_set_state(talp_info_t *talp_info, talp_sample_state_t new_state) {

    talp_sample_t *sample = talp_sample_get(talp_info);
    set_state(talp_info, sample, new_state);
}


/*********************************************************************************/
/*    Sample aggregation                                                         */
/*********************************************************************************/

/* Flush and aggregate a single sample into a macrosample */
static inline void flush_sample_to_macrosample(talp_sample_t *restrict sample,
        talp_macrosample_t *restrict macrosample) {

    /* Timers */
    macrosample->timers.useful +=
        DLB_ATOMIC_EXCH_RLX(&sample->timers.useful, 0);
    macrosample->timers.not_useful_mpi +=
        DLB_ATOMIC_EXCH_RLX(&sample->timers.not_useful_mpi, 0);
    macrosample->timers.not_useful_omp_during_mpi +=
        DLB_ATOMIC_EXCH_RLX(&sample->timers.not_useful_omp_during_mpi, 0);
    macrosample->timers.not_useful_omp_out +=
        DLB_ATOMIC_EXCH_RLX(&sample->timers.not_useful_omp_out, 0);
    /* timers.not_useful_omp_in is not flushed here, make sure struct is empty */
    ensure(DLB_ATOMIC_LD_RLX(&sample->timers.not_useful_omp_in) == 0,
                "Inconsistency in TALP sample metric not_useful_omp_in."
                " Please, report bug at " PACKAGE_BUGREPORT);
    macrosample->timers.not_useful_gpu +=
        DLB_ATOMIC_EXCH_RLX(&sample->timers.not_useful_gpu, 0);

    /* Counters */
    macrosample->counters.cycles +=
        DLB_ATOMIC_EXCH_RLX(&sample->counters.cycles, 0);
    macrosample->counters.instructions +=
        DLB_ATOMIC_EXCH_RLX(&sample->counters.instructions, 0);

    /* Stats */
    macrosample->stats.num_mpi_calls +=
        DLB_ATOMIC_EXCH_RLX(&sample->stats.num_mpi_calls, 0);
    macrosample->stats.num_omp_parallels +=
        DLB_ATOMIC_EXCH_RLX(&sample->stats.num_omp_parallels, 0);
    macrosample->stats.num_omp_tasks +=
        DLB_ATOMIC_EXCH_RLX(&sample->stats.num_omp_tasks, 0);
    macrosample->stats.num_gpu_runtime_calls +=
        DLB_ATOMIC_EXCH_RLX(&sample->stats.num_gpu_runtime_calls, 0);
}


/* Aggregate all samples.
 * This function assumes that the current thread's sample was just updated. */
void talp_sample_aggregate_all_to_macrosample(
        talp_info_t *restrict talp_info, talp_macrosample_t *restrict macrosample) {

    sample_registry_t *registry = &talp_info->sample_registry;
    talp_sample_t *current_sample = talp_sample_get(talp_info);
    int64_t now = current_sample->last_updated_ts;

    /* Accumulate samples from all threads */
    pthread_mutex_lock(&registry->mutex);
    {
        int num_samples = registry->num_samples;
        macrosample->num_cpus = num_samples;

        /* Force-update and aggregate all samples */
        for (int i = 0; i < num_samples; ++i) {
            talp_sample_t *sample = registry->samples[i];
            if (!talp_sample_is_mine(sample)) {
                talp_sample_update_foreign(talp_info, sample, now);
            }
            flush_sample_to_macrosample(sample, macrosample);
        }
    }
    pthread_mutex_unlock(&registry->mutex);
}

/* Aggregate a subset of samples.
 * This function assumes that the current thread's sample was just updated.
 * OpenMP derived metrics are computed here and added to the main sample. */
void talp_sample_aggregate_subset_to_macrosample(
        talp_info_t *restrict talp_info,
        talp_sample_t **restrict samples,
        unsigned int nelems,
        talp_macrosample_t *restrict macrosample) {

    sample_registry_t *registry = &talp_info->sample_registry;
    talp_sample_t *sample = talp_sample_get(talp_info);
    int64_t now = sample->last_updated_ts;

    int64_t sched_timer = 0;
    int64_t lb_timer = 0;

    pthread_mutex_lock(&registry->mutex);
    {
        /* Iterate first to force-update all samples and compute the minimum
         * not-useful-omp-in among them */
        int64_t min_not_useful_omp_in = INT64_MAX;
        for (unsigned int i = 0; i < nelems; ++i) {
            talp_sample_t *worker_sample = samples[i];
            if (!talp_sample_is_mine(worker_sample)) {
                talp_sample_update_foreign(talp_info, worker_sample, now);
            }
            min_not_useful_omp_in = min_int64(min_not_useful_omp_in,
                    DLB_ATOMIC_LD_RLX(&worker_sample->timers.not_useful_omp_in));
        }

        /* Iterate again to accumulate Load Balance, and to aggregate sample */
        sched_timer = min_not_useful_omp_in * nelems;
        for (unsigned int i = 0; i < nelems; ++i) {
            talp_sample_t *worker_sample = samples[i];
            lb_timer += DLB_ATOMIC_EXCH_RLX(&worker_sample->timers.not_useful_omp_in, 0)
                - min_not_useful_omp_in;
            flush_sample_to_macrosample(worker_sample, macrosample);
        }
    }
    pthread_mutex_unlock(&registry->mutex);

    /* Update derived timers into macrosample */
    macrosample->num_cpus = nelems;
    macrosample->timers.not_useful_omp_in_lb = lb_timer;
    macrosample->timers.not_useful_omp_in_sched = sched_timer;
}
