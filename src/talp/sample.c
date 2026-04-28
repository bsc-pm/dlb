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
extern __thread bool thread_is_known;

static __thread talp_sample_t* _tls_sample = NULL;
static __thread bool _is_main_sample = false;
static __thread bool _is_main_sample_in_sequential_mode = false;

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

bool talp_sample_is_main_sequential(void) {
    return _is_main_sample_in_sequential_mode;
}

/* Sets the TLS variable _is_main_sample_in_sequential_mode. This function is
 * called by the main thread when beginning or ending parallel region of level 1.
 * FIXME: free agent threads may break this condition.
 *
 * Sets whether the main thread is running in serial mode. */
void talp_sample_set_main_sequential(bool is_sequential) {

    if (_is_main_sample) {
        _is_main_sample_in_sequential_mode = is_sequential;
    }
}

/* Get the TLS associated sample */
talp_sample_t* talp_sample_get(talp_info_t *talp_info) {

    /* Thread already has an allocated sample, return it */
    if (likely(_tls_sample != NULL)) return _tls_sample;

    /* Observer and unknown threads don't have a valid sample */
    if (unlikely(thread_is_observer || !thread_is_known)) return NULL;

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
                    _is_main_sample_in_sequential_mode = true;
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
        const dlb_monitor_t *innermost_monitor = talp_info->open_regions->data;
        last_updated_ts = innermost_monitor->start_time;
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

/* Reset sample metrics */
static inline void reset_sample(talp_sample_t *sample) {
    memset(&sample->timers,   0, sizeof(sample->timers));
    memset(&sample->counters, 0, sizeof(sample->counters));
    memset(&sample->stats,    0, sizeof(sample->stats));
}


/*********************************************************************************/
/*    Sample update                                                              */
/*********************************************************************************/

static inline void ensure_generation(talp_sample_t *sample, int64_t current_generation_ts) {
    if (unlikely(sample->generation_ts < current_generation_ts)) {
        reset_sample(sample);
        sample->generation_ts = current_generation_ts;
        sample->last_updated_ts = current_generation_ts;
    }
}

/* Compute new microsample (time since last update) and update sample values */
void talp_sample_update(talp_info_t *talp_info) {

    talp_sample_t *sample = talp_sample_get(talp_info);
    sample_registry_t *registry = &talp_info->sample_registry;

    /* Observer and unknown threads ignore this function */
    if (unlikely(sample == NULL)) return;

    int64_t current_generation = DLB_ATOMIC_LD_RLX(&registry->current_generation_ts);

    /* Generation helps us to identify whether this sample belongs to the
     * current generation (i.e., region has yet to be updated) or if we need to
     * reset it */
    ensure_generation(sample, current_generation);

    /* Compute duration and set new last_updated_ts */
    int64_t now = get_time_in_ns();
    int64_t microsample_duration = now - sample->last_updated_ts;
    sample->last_updated_ts = now;

    /* Update the appropriate sample timer */
    switch(sample->state) {
        case TALP_STATE_DISABLED:
            break;
        case TALP_STATE_USEFUL:
            sample->timers.useful += microsample_duration;
            break;
        case TALP_STATE_NOT_USEFUL_MPI:
            sample->timers.not_useful_mpi += microsample_duration;
            if (_is_main_sample_in_sequential_mode) {
                // Add worker threads' time to special timer
                int num_cpus = talp_info->sample_registry.num_samples;
                sample->timers.not_useful_omp_during_mpi
                    += microsample_duration * (num_cpus-1);
            }
            break;
        case TALP_STATE_NOT_USEFUL_OMP_IN:
            sample->timers.not_useful_omp_in += microsample_duration;
            break;
        case TALP_STATE_NOT_USEFUL_OMP_OUT:
            sample->timers.not_useful_omp_out += microsample_duration;
            break;
        case TALP_STATE_NOT_USEFUL_GPU:
            sample->timers.not_useful_gpu += microsample_duration;
            break;
    }

    if (talp_info->flags.have_hwc) {
        hwc_measurements_t measurements;
        if (talp_hwc_collect(&measurements)) {
            sample->counters.cycles       += measurements.cycles;
            sample->counters.instructions += measurements.instructions;
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

/* Aggregate a single sample into a macrosample */
static inline void aggregate_sample_to_macrosample(const talp_sample_t *restrict sample,
        talp_macrosample_t *restrict macrosample) {

    /* Timers */
    macrosample->timers.useful                    += sample->timers.useful;
    macrosample->timers.not_useful_mpi            += sample->timers.not_useful_mpi;
    macrosample->timers.not_useful_omp_during_mpi += sample->timers.not_useful_omp_during_mpi;
    macrosample->timers.not_useful_omp_in_lb      += sample->timers.not_useful_omp_in_lb;
    macrosample->timers.not_useful_omp_in_sched   += sample->timers.not_useful_omp_in_sched;
    macrosample->timers.not_useful_omp_out        += sample->timers.not_useful_omp_out;
    macrosample->timers.not_useful_gpu            += sample->timers.not_useful_gpu;

    /* Counters */
    macrosample->counters.cycles                  += sample->counters.cycles;
    macrosample->counters.instructions            += sample->counters.instructions;

    /* Stats */
    macrosample->stats.num_mpi_calls              += sample->stats.num_mpi_calls;
    macrosample->stats.num_omp_parallels          += sample->stats.num_omp_parallels;
    macrosample->stats.num_omp_tasks              += sample->stats.num_omp_tasks;
    macrosample->stats.num_gpu_runtime_calls      += sample->stats.num_gpu_runtime_calls;
}


/* Aggregate all samples. Don't update any. */
void talp_sample_aggregate_all_to_macrosample(
        talp_info_t *restrict talp_info, talp_macrosample_t *restrict macrosample) {

    /* Warning: observer threads can call this function although is prone to
     * produce some race conditions while reading samples. Still, we cannot
     * assume samples[0] is our sample */
    talp_sample_t *my_sample = talp_sample_get(talp_info);

    /* If this function is called by the main thread (expected),
     * re-use the timestamp that was just set updating the sample. */
    int64_t now = my_sample != NULL
        ? my_sample->last_updated_ts
        : get_time_in_ns();

    sample_registry_t *registry = &talp_info->sample_registry;
    int64_t current_generation_ts = DLB_ATOMIC_LD_RLX(&registry->current_generation_ts);
    int64_t generation_duration = now - current_generation_ts;

    /* Accumulate samples from all threads */
    pthread_mutex_lock(&registry->mutex);
    {
        int num_samples = registry->num_samples;

        macrosample->num_cpus = num_samples;

        /* Aggregate samples */
        for (int i = 0; i < num_samples; ++i) {
            const talp_sample_t *sample = registry->samples[i];

            if (sample == my_sample) {
                /* Our sample is just aggregated because we know it's updated */
                aggregate_sample_to_macrosample(sample, macrosample);
                continue;
            }

            /* Note: By contract, we only aggregate samples in sequential code.
             * So at this point, we only look for threads that are probably
             * stopped after a parallel region.
             * Since implicit-task-end event is not reliable, we use the
             * last_parallel_end_ts set by the primary to compute the missing
             * not-useful-omp-out here */

            if (likely(sample->state == TALP_STATE_NOT_USEFUL_OMP_IN
                        || sample->state == TALP_STATE_NOT_USEFUL_OMP_OUT)) {

                int64_t last_parallel_end_ts = DLB_ATOMIC_LD_RLX(&sample->last_parallel_end_ts);
                macrosample->timers.not_useful_omp_out += min_int64(
                        generation_duration,
                        now - last_parallel_end_ts);
            }

            if (sample->generation_ts < current_generation_ts) {
                /* Sample not updated in the current generation.
                 * Skip aggregation of any other timer.*/
                continue;
            }

            /* Aggregate the values computed by the thread */
            aggregate_sample_to_macrosample(sample, macrosample);
        }
    }
    pthread_mutex_unlock(&registry->mutex);

    /* Start generation for the next sample aggregation */
    DLB_ATOMIC_ST_RLX(&registry->current_generation_ts, now);

    /* If this function is called by the main thread (expected),
     * reset sample and set the new generation time-stamp. */
    if (my_sample != NULL) {
        reset_sample(my_sample);
        my_sample->generation_ts = now;
    }
}
