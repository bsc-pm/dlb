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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "talp/talp.h"

#include "LB_core/node_barrier.h"
#include "LB_core/spd.h"
#include "LB_comm/shmem_talp.h"
#include "apis/dlb_errors.h"
#include "apis/dlb_talp.h"
#include "support/atomic.h"
#include "support/debug.h"
#include "support/error.h"
#include "support/gslist.h"
#include "support/gtree.h"
#include "support/mytime.h"
#include "support/tracing.h"
#include "support/options.h"
#include "support/mask_utils.h"
#include "talp/backend.h"
#include "talp/perf_metrics.h"
#include "talp/regions.h"
#include "talp/talp_gpu.h"
#include "talp/talp_hwc.h"
#include "talp/talp_output.h"
#include "talp/talp_record.h"
#include "talp/talp_types.h"
#ifdef MPI_LIB
#include "mpi/mpi_core.h"
#endif

#include <stdlib.h>
#include <pthread.h>

extern __thread bool thread_is_observer;

static void talp_dealloc_samples(const subprocess_descriptor_t *spd);


/* Update all open regions with the macrosample */
static void update_regions_with_macrosample(const subprocess_descriptor_t *spd,
        const talp_macrosample_t *macrosample, int num_cpus) {
    talp_info_t *talp_info = spd->talp_info;

    /* Update all open regions */
    pthread_mutex_lock(&talp_info->regions_mutex);
    {
        for (GSList *node = talp_info->open_regions;
                node != NULL;
                node = node->next) {
            dlb_monitor_t *monitor = node->data;
            monitor_data_t *monitor_data = monitor->_data;

            /* Update number of CPUs if needed */
            monitor->num_cpus = max_int(monitor->num_cpus, num_cpus);

            /* Timers */
            monitor->useful_time += macrosample->timers.useful;
            monitor->mpi_time += macrosample->timers.not_useful_mpi;
            monitor->omp_load_imbalance_time += macrosample->timers.not_useful_omp_in_lb;
            monitor->omp_scheduling_time += macrosample->timers.not_useful_omp_in_sched;
            monitor->omp_serialization_time += macrosample->timers.not_useful_omp_out;
            monitor->gpu_runtime_time += macrosample->timers.not_useful_gpu;

            /* GPU Timers */
            monitor->gpu_useful_time += macrosample->gpu_timers.useful;
            monitor->gpu_communication_time += macrosample->gpu_timers.communication;
            monitor->gpu_inactive_time += macrosample->gpu_timers.inactive;

            /* Counters */
            monitor->cycles += macrosample->counters.cycles;
            monitor->instructions += macrosample->counters.instructions;

            /* Stats */
            monitor->num_mpi_calls += macrosample->stats.num_mpi_calls;
            monitor->num_omp_parallels += macrosample->stats.num_omp_parallels;
            monitor->num_omp_tasks += macrosample->stats.num_omp_tasks;
            monitor->num_gpu_runtime_calls += macrosample->stats.num_gpu_runtime_calls;

            /* Update shared memory only if requested */
            if (talp_info->flags.external_profiler) {
                shmem_talp__set_times(monitor_data->node_shared_id,
                        monitor->mpi_time,
                        monitor->useful_time);
            }
        }
    }
    pthread_mutex_unlock(&talp_info->regions_mutex);
}


#ifdef MPI_LIB
/* Returns the number of MPI processes that have HWC enabled */
static int get_hwc_init_across_world(const subprocess_descriptor_t *spd) {

    talp_info_t *talp_info = spd->talp_info;

    // status = 1 means HWC are enabled
    int hwc_local_status = talp_info->flags.have_hwc ? 1 : 0;

    int hwc_global_statuses = 0;

    PMPI_Allreduce(&hwc_local_status, &hwc_global_statuses, 1,
            MPI_INT, MPI_SUM, getWorldComm());

    return hwc_global_statuses;
}
#endif


/*********************************************************************************/
/*    Init / Finalize                                                            */
/*********************************************************************************/

void talp_init(subprocess_descriptor_t *spd) {

    ensure(!spd->talp_info, "TALP already initialized");
    ensure(!thread_is_observer, "An observer thread cannot call talp_init");
    verbose(VB_TALP, "Initializing TALP module with worker mask: %s",
            mu_to_str(&spd->process_mask));

    /* Initialize talp info */
    talp_info_t *talp_info = malloc(sizeof(talp_info_t));
    *talp_info = (const talp_info_t) {
        .flags = {
            .external_profiler = spd->options.talp_external_profiler,
            .have_shmem = spd->options.talp_external_profiler,
            .have_minimal_shmem = !spd->options.talp_external_profiler
                && spd->options.talp_summary & SUMMARY_NODE,
        },
        .regions = g_tree_new_full(
                (GCompareDataFunc)region_compare_by_name,
                NULL, NULL, region_dealloc),
        .regions_mutex = PTHREAD_MUTEX_INITIALIZER,
        .samples_mutex = PTHREAD_MUTEX_INITIALIZER,
    };
    spd->talp_info = talp_info;

    /* Initialize shared memory */
    if (talp_info->flags.have_shmem || talp_info->flags.have_minimal_shmem) {
        /* If we only need a minimal shmem, its size will be the user-provided
         * multiplier times 'system_size' (usually, 1 region per process)
         * Otherwise, we multiply it by DEFAULT_REGIONS_PER_PROC.
         */
        enum { DEFAULT_REGIONS_PER_PROC = 100 };
        int shmem_size_multiplier = spd->options.shm_size_multiplier
            * (talp_info->flags.have_shmem ? DEFAULT_REGIONS_PER_PROC : 1);
        shmem_talp__init(spd->options.shm_key, shmem_size_multiplier);
    }

    /* Initialize TALP components */
    if (spd->options.talp & (TALP_COMPONENT_DEFAULT | TALP_COMPONENT_GPU)) {
        if (talp_gpu_init(spd) == DLB_SUCCESS) {
            talp_info->flags.have_gpu = true;
            verbose(VB_TALP, "GPU component enabled successfully");
        } else {
            if (spd->options.talp & TALP_COMPONENT_GPU) {
                /* component was explicit and failed, warn user */
                warning("TALP: Failed to load GPU component");
            }
        }
    }
    if (spd->options.talp & (TALP_COMPONENT_DEFAULT | TALP_COMPONENT_HWC)) {
        if (talp_hwc_init(spd) == DLB_SUCCESS) {
            talp_info->flags.have_hwc = true;
            verbose(VB_TALP, "HWC component enabled successfully");
        } else {
            if (spd->options.talp & TALP_COMPONENT_HWC) {
                /* component was explicit and failed, warn user */
                warning("TALP: Failed to load HWC component");
            }
        }
    }

#ifdef MPI_LIB
    /* Check HWC status across all process. Every process needs to do the check
     * because it's a collective operation and some process may have been started
     * without the appropriate flag. */
    if (is_mpi_ready()) {
        int num_procs_with_hwc = get_hwc_init_across_world(spd);
        if (num_procs_with_hwc > 0 && num_procs_with_hwc < _mpi_size) {
            warning0("Hardware Counters initialization has failed, disabling option.");
            talp_hwc_finalize();
            talp_info->flags.have_hwc = false;
        }
    }
#endif

    /* Initialize global region monitor
     * (at this point we don't know how many CPUs, it will be fixed in talp_openmp_init) */
    talp_info->monitor = region_register(spd, region_get_global_name());

    /* Start global region */
    region_start(spd, talp_info->monitor);
}

void talp_finalize(subprocess_descriptor_t *spd) {
    ensure(spd->talp_info, "TALP is not initialized");
    ensure(!thread_is_observer, "An observer thread cannot call talp_finalize");
    verbose(VB_TALP, "Finalizing TALP module");

    talp_info_t *talp_info = spd->talp_info;

    /* Stop open regions
     * (Note that region_stop need to acquire the regions_mutex
     * lock, so we we need to iterate without it) */
    while(talp_info->open_regions != NULL) {
        dlb_monitor_t *monitor = talp_info->open_regions->data;
        region_stop(spd, monitor);
    }

    /* Finalize TALP components */
    if (talp_info->flags.have_gpu) {
        talp_gpu_finalize();
    }
    if (talp_info->flags.have_hwc) {
        talp_hwc_finalize();
    }

    /* Per-process output (no MPI or requested by user) */
    if (!talp_info->flags.have_mpi
            || spd->options.talp_partial_output) {

        pthread_mutex_lock(&talp_info->regions_mutex);
        {
            /* Record all regions */
            for (GTreeNode *node = g_tree_node_first(talp_info->regions);
                    node != NULL;
                    node = g_tree_node_next(node)) {
                const dlb_monitor_t *monitor = g_tree_node_value(node);
                talp_record_monitor(spd, monitor);
            }
        }
        pthread_mutex_unlock(&talp_info->regions_mutex);
    }

    /* Print/write all collected summaries */
    talp_output_finalize(spd->options.talp_output_file, spd->options.talp_partial_output);

    /* Deallocate samples structure */
    talp_dealloc_samples(spd);

    /* Finalize shared memory */
    if (talp_info->flags.have_shmem || talp_info->flags.have_minimal_shmem) {
        shmem_talp__finalize(spd->id);
    }

    /* Deallocate monitoring regions and talp_info */
    pthread_mutex_lock(&talp_info->regions_mutex);
    {
        /* Destroy GTree, each node is deallocated with the function region_dealloc */
        g_tree_destroy(talp_info->regions);
        talp_info->regions = NULL;
        talp_info->monitor = NULL;

        /* Destroy list of open regions */
        g_slist_free(talp_info->open_regions);
        talp_info->open_regions = NULL;
    }
    pthread_mutex_unlock(&talp_info->regions_mutex);
    free(talp_info);
    spd->talp_info = NULL;
}


/*********************************************************************************/
/*    Sample functions                                                           */
/*********************************************************************************/

static __thread talp_sample_t* _tls_sample = NULL;
static __thread bool _is_main_sample = false;
static __thread bool _is_main_sample_in_serial_mode = false;

/* Quick test, without locking and without generating a new sample */
static inline bool is_talp_sample_mine(const talp_sample_t *sample) {
    return sample != NULL && sample == _tls_sample;
}

static void talp_dealloc_samples(const subprocess_descriptor_t *spd) {

    /* Warning about _tls_sample in worker threads:
     * worker threads do not currently deallocate their sample.
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
    talp_info_t *talp_info = spd->talp_info;
    pthread_mutex_lock(&talp_info->samples_mutex);
    {
        free(talp_info->samples);
        talp_info->samples = NULL;
        talp_info->ncpus = 0;
    }
    pthread_mutex_unlock(&talp_info->samples_mutex);
}

/* Get the TLS associated sample */
talp_sample_t* talp_get_thread_sample(const subprocess_descriptor_t *spd) {

    /* Thread already has an allocated sample, return it */
    if (likely(_tls_sample != NULL)) return _tls_sample;

    /* Observer threads don't have a valid sample */
    if (unlikely(thread_is_observer)) return NULL;

    /* Otherwise, allocate */
    talp_info_t *talp_info = spd->talp_info;
    pthread_mutex_lock(&talp_info->samples_mutex);
    {
        int ncpus = ++talp_info->ncpus;
        void *samples = realloc(talp_info->samples, sizeof(talp_sample_t*)*ncpus);
        if (samples) {
            talp_info->samples = samples;
            void *new_sample;
            if (posix_memalign(&new_sample, DLB_CACHE_LINE, sizeof(talp_sample_t)) == 0) {
                _tls_sample = new_sample;
                talp_info->samples[ncpus-1] = new_sample;
                if (ncpus == 1) {
                    _is_main_sample = true;
                    _is_main_sample_in_serial_mode = true;
                }
            }
        }
    }
    pthread_mutex_unlock(&talp_info->samples_mutex);

    fatal_cond(_tls_sample == NULL, "TALP: could not allocate thread sample");

    /* If a thread is created mid-region, its initial time is that of the
     * innermost open region, otherwise it is the current time */
    int64_t last_updated_timestamp;
    if (talp_info->open_regions) {
        const dlb_monitor_t *monitor = talp_info->open_regions->data;
        last_updated_timestamp = monitor->start_time;
    } else {
        last_updated_timestamp = get_time_in_ns();
    }

    *_tls_sample = (const talp_sample_t) {
        .last_updated_timestamp = last_updated_timestamp,
    };

    talp_set_sample_state(spd, _tls_sample, TALP_STATE_DISABLED);

#ifdef INSTRUMENTATION_VERSION
    unsigned events[] = {MONITOR_CYCLES, MONITOR_INSTR};
    long long hwc_values[] = {0, 0};
    instrument_nevent(2, events, hwc_values);
#endif

    return _tls_sample;
}

/* WARNING: this function may only be called when updating own thread's sample */
void talp_set_sample_state(const subprocess_descriptor_t *spd, talp_sample_t *sample,
        talp_sample_state_t new_state) {

    talp_info_t *talp_info = spd->talp_info;
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

/* Compute new microsample (time since last update) and update sample values */
void talp_update_sample(const subprocess_descriptor_t *spd, talp_sample_t *sample,
        int64_t timestamp) {

    /* Observer threads ignore this function */
    if (unlikely(sample == NULL)) return;

    talp_info_t *talp_info = spd->talp_info;

    /* Compute duration and set new last_updated_timestamp */
    int64_t now = timestamp == TALP_NO_TIMESTAMP ? get_time_in_ns() : timestamp;
    int64_t microsample_duration = now - sample->last_updated_timestamp;
    sample->last_updated_timestamp = now;

    /* Update the appropriate sample timer */
    switch(sample->state) {
        case TALP_STATE_DISABLED:
            break;
        case TALP_STATE_USEFUL:
            DLB_ATOMIC_ADD_RLX(&sample->timers.useful, microsample_duration);
            break;
        case TALP_STATE_NOT_USEFUL_MPI:
            if (_is_main_sample_in_serial_mode) {
                int num_cpus = talp_info->ncpus;
                microsample_duration *= num_cpus;
            }
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

    if (talp_info->flags.have_hwc) {
        /* Only read counters if we are updating this thread's sample */
        if (is_talp_sample_mine(sample)) {
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
}

/* Flush and aggregate a single sample into a macrosample */
static inline void flush_sample_to_macrosample(talp_sample_t *sample,
        talp_macrosample_t *macrosample) {

    /* Timers */
    macrosample->timers.useful +=
        DLB_ATOMIC_EXCH_RLX(&sample->timers.useful, 0);
    macrosample->timers.not_useful_mpi +=
        DLB_ATOMIC_EXCH_RLX(&sample->timers.not_useful_mpi, 0);
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

/* Accumulate values from samples of all threads and update regions */
int talp_flush_samples_to_regions(const subprocess_descriptor_t *spd) {

    /* Observer threads don't have a valid sample so they cannot start/stop regions */
    if (unlikely(thread_is_observer)) return DLB_ERR_PERM;

    int num_cpus;
    talp_info_t *talp_info = spd->talp_info;

    /* Accumulate samples from all threads */
    talp_macrosample_t macrosample = (const talp_macrosample_t) {};
    pthread_mutex_lock(&talp_info->samples_mutex);
    {
        num_cpus = talp_info->ncpus;

        /* Force-update and aggregate all samples */
        int64_t timestamp = get_time_in_ns();
        for (int i = 0; i < num_cpus; ++i) {
            talp_update_sample(spd, talp_info->samples[i], timestamp);
            flush_sample_to_macrosample(talp_info->samples[i], &macrosample);
        }
    }
    pthread_mutex_unlock(&talp_info->samples_mutex);

    if (talp_info->flags.have_gpu) {
        /* Collect GPU measuremnts up to this point and update macrosample */
        gpu_measurements_t measurements;
        talp_gpu_collect(&measurements);
        macrosample.gpu_timers.useful        = measurements.useful_time;
        macrosample.gpu_timers.communication = measurements.communication_time;
        macrosample.gpu_timers.inactive      = measurements.inactive_time;
    }

    /* Update all started regions */
    update_regions_with_macrosample(spd, &macrosample, num_cpus);

    return DLB_SUCCESS;
}

/* Accumulate samples from only a subset of samples of a parallel region.
 * Load Balance and Scheduling are computed here based on all samples. */
void talp_flush_sample_subset_to_regions(const subprocess_descriptor_t *spd,
        talp_sample_t **samples, unsigned int nelems) {

    talp_info_t *talp_info = spd->talp_info;
    talp_macrosample_t macrosample = (const talp_macrosample_t) {};
    pthread_mutex_lock(&talp_info->samples_mutex);
    {
        /* Iterate first to force-update all samples and compute the minimum
         * not-useful-omp-in among them */
        int64_t timestamp = get_time_in_ns();
        int64_t min_not_useful_omp_in = INT64_MAX;
        unsigned int i;
        for (i=0; i<nelems; ++i) {
            talp_update_sample(spd, samples[i], timestamp);
            min_not_useful_omp_in = min_int64(min_not_useful_omp_in,
                    DLB_ATOMIC_LD_RLX(&samples[i]->timers.not_useful_omp_in));
        }

        /* Iterate again to accumulate Load Balance, and to aggregate sample */
        int64_t sched_timer = min_not_useful_omp_in * nelems;
        int64_t lb_timer = 0;
        for (i=0; i<nelems; ++i) {
            lb_timer += DLB_ATOMIC_EXCH_RLX(&samples[i]->timers.not_useful_omp_in, 0)
                - min_not_useful_omp_in;
            flush_sample_to_macrosample(samples[i], &macrosample);
        }

        /* Update derived timers into macrosample */
        macrosample.timers.not_useful_omp_in_lb = lb_timer;
        macrosample.timers.not_useful_omp_in_sched = sched_timer;
    }
    pthread_mutex_unlock(&talp_info->samples_mutex);

    /* Update all started regions */
    update_regions_with_macrosample(spd, &macrosample, nelems);
}

/* Sets the TLS variable _is_main_sample_in_serial_mode. This function is
 * called by the main thread when beginning or ending parallel region of level 1.
 * FIXME: free agent threads may break this condition.
 *
 * Sets whether the main thread is running in serial mode. */
void talp_set_main_sample_in_serial_mode(bool serial_mode) {

    if (_is_main_sample) {
        _is_main_sample_in_serial_mode = serial_mode;
    }
}


/*********************************************************************************/
/*    TALP collect functions for 3rd party programs:                             */
/*      - It's also safe to call it from a 1st party program                     */
/*      - Requires --talp-external-profiler set up in application                */
/*      - Does not need to synchronize with application                           */
/*********************************************************************************/

/* Function that may be called from a third-party process to compute
 * node_metrics for a given region */
int talp_query_pop_node_metrics(const char *name, dlb_node_metrics_t *node_metrics) {

    if (name == NULL) {
        name = region_get_global_name();
    }

    int error = DLB_SUCCESS;
    int64_t total_mpi_time = 0;
    int64_t total_useful_time = 0;
    int64_t max_mpi_time = 0;
    int64_t max_useful_time = 0;

    /* Obtain a list of regions in the node associated with given region */
    int max_procs = mu_get_system_size();
    talp_region_list_t *region_list = malloc(max_procs * sizeof(talp_region_list_t));
    int nelems;
    shmem_talp__get_regionlist(region_list, &nelems, max_procs, name);

    /* Count how many processes have started the region */
    int processes_per_node = 0;

    /* Iterate the PID list and gather times of every process */
    int i;
    for (i = 0; i <nelems; ++i) {
        int64_t mpi_time = region_list[i].mpi_time;
        int64_t useful_time = region_list[i].useful_time;

        /* Accumulate total and max values */
        if (mpi_time > 0 || useful_time > 0) {
            ++processes_per_node;
            total_mpi_time += mpi_time;
            total_useful_time += useful_time;
            max_mpi_time = max_int64(mpi_time, max_mpi_time);
            max_useful_time = max_int64(useful_time, max_useful_time);
        }
    }
    free(region_list);

#if MPI_LIB
    int node_id = _node_id;
#else
    int node_id = 0;
#endif

    if (processes_per_node > 0) {
        /* Compute POP metrics with some inferred values */
        perf_metrics_mpi_t metrics;
        perf_metrics__infer_mpi_model(
                &metrics,
                processes_per_node,
                total_useful_time,
                total_mpi_time,
                max_useful_time);

        /* Initialize structure */
        *node_metrics = (const dlb_node_metrics_t) {
            .node_id = node_id,
            .processes_per_node = processes_per_node,
            .total_useful_time = total_useful_time,
            .total_mpi_time = total_mpi_time,
            .max_useful_time = max_useful_time,
            .max_mpi_time = max_mpi_time,
            .parallel_efficiency = metrics.parallel_efficiency,
            .communication_efficiency = metrics.communication_efficiency,
            .load_balance = metrics.load_balance,
        };
        snprintf(node_metrics->name, DLB_MONITOR_NAME_MAX, "%s", name);
    } else {
        error = DLB_ERR_NOENT;
    }

    return error;
}


/*********************************************************************************/
/*    TALP collect functions for 1st party programs                              */
/*      - Requires synchronization (MPI or node barrier) among all processes     */
/*********************************************************************************/

/* Compute the current POP metrics for the specified monitor. If monitor is NULL,
 * the global monitoring region is assumed.
 * Pre-conditions:
 *  - if MPI, the given monitor must have been registered in all MPI ranks
 *  - pop_metrics is an allocated structure
 */
int talp_collect_pop_metrics(const subprocess_descriptor_t *spd,
        dlb_monitor_t *monitor, dlb_pop_metrics_t *pop_metrics) {
    talp_info_t *talp_info = spd->talp_info;
    if (monitor == NULL) {
        monitor = talp_info->monitor;
    }

    /* Stop monitor so that metrics are updated */
    bool resume_region = region_stop(spd, monitor) == DLB_SUCCESS;

    pop_base_metrics_t base_metrics;
#ifdef MPI_LIB
    /* Reduce monitor among all MPI ranks and everbody collects (all-to-all) */
    perf_metrics__reduce_monitor_into_base_metrics(&base_metrics, monitor, true);
#else
    /* Construct base metrics using only the monitor from this process */
    perf_metrics__local_monitor_into_base_metrics(&base_metrics, monitor);
#endif

    /* Construct output pop_metrics out of base metrics */
    perf_metrics__base_to_pop_metrics(monitor->name, &base_metrics, pop_metrics);

    /* Resume monitor */
    if (resume_region) {
        region_start(spd, monitor);
    }

    return DLB_SUCCESS;
}

/* Node-collective function to compute node_metrics for a given region */
int talp_collect_pop_node_metrics(const subprocess_descriptor_t *spd,
        dlb_monitor_t *monitor, dlb_node_metrics_t *node_metrics) {

    talp_info_t *talp_info = spd->talp_info;
    monitor = monitor ? monitor : talp_info->monitor;
    monitor_data_t *monitor_data = monitor->_data;

    /* Stop monitor so that metrics are updated */
    bool resume_region = region_stop(spd, monitor) == DLB_SUCCESS;

    /* This functionality needs a shared memory, create a temporary one if needed */
    if (!talp_info->flags.have_shmem) {
        shmem_talp__init(spd->options.shm_key, 1);
        shmem_talp__register(spd->id, monitor->avg_cpus, monitor->name,
                &monitor_data->node_shared_id);
    }

    /* Update the shared memory with this process' metrics */
    shmem_talp__set_times(monitor_data->node_shared_id,
            monitor->mpi_time,
            monitor->useful_time);

    /* Perform a node barrier to ensure everyone has updated their metrics */
    node_barrier(spd, NULL);

    /* Compute node metrics for that region name */
    talp_query_pop_node_metrics(monitor->name, node_metrics);

    /* Remove shared memory if it was a temporary one */
    if (!talp_info->flags.have_shmem) {
        shmem_talp__finalize(spd->id);
    }

    /* Resume monitor */
    if (resume_region) {
        region_start(spd, monitor);
    }

    return DLB_SUCCESS;
}
