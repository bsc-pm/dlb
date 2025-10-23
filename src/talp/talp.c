/*********************************************************************************/
/*  Copyright 2009-2025 Barcelona Supercomputing Center                          */
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
#include "talp/perf_metrics.h"
#include "talp/regions.h"
#include "talp/talp_gpu.h"
#include "talp/talp_output.h"
#include "talp/talp_record.h"
#include "talp/talp_types.h"
#ifdef MPI_LIB
#include "mpi/mpi_core.h"
#endif

#include <stdlib.h>
#include <pthread.h>

#ifdef PAPI_LIB
#include <papi.h>
#endif

extern __thread bool thread_is_observer;

static void talp_dealloc_samples(const subprocess_descriptor_t *spd);


#if PAPI_LIB
static __thread int EventSet = PAPI_NULL;
#endif


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

#ifdef PAPI_LIB
            /* Counters */
            monitor->cycles += macrosample->counters.cycles;
            monitor->instructions += macrosample->counters.instructions;
#endif
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



/*********************************************************************************/
/*    PAPI counters                                                              */
/*********************************************************************************/

/* Called once in the main thread */
static inline int init_papi(void) __attribute__((unused));
static inline int init_papi(void) {
#ifdef PAPI_LIB
    ensure( ((talp_info_t*)thread_spd->talp_info)->flags.papi,
            "Error invoking %s when PAPI has been disabled", __FUNCTION__);

    /* Library init */
    int retval = PAPI_library_init(PAPI_VER_CURRENT);
    if(retval != PAPI_VER_CURRENT || retval == PAPI_EINVAL){
        // PAPI init failed because of a version mismatch
        // Maybe we can rectify the situation by cheating a bit.
        // Lets first check if the major version is the same
        int loaded_lib_version = PAPI_get_opt(PAPI_LIB_VERSION, NULL);
        if(PAPI_VERSION_MAJOR(PAPI_VER_CURRENT) == PAPI_VERSION_MAJOR(loaded_lib_version)){
            warning("The PAPI version loaded at runtime differs from the one DLB was compiled with. Expected version %d.%d.%d but got %d.%d.%d. Continuing on best effort.",
                PAPI_VERSION_MAJOR(PAPI_VER_CURRENT),
                PAPI_VERSION_MINOR(PAPI_VER_CURRENT),
                PAPI_VERSION_REVISION(PAPI_VER_CURRENT),
                PAPI_VERSION_MAJOR(loaded_lib_version),
                PAPI_VERSION_MINOR(loaded_lib_version),
                PAPI_VERSION_REVISION(loaded_lib_version));
            // retval here can only be loaded_lib_version or negative. We assume loaded_lib_version and check for other failures below
            retval = PAPI_library_init(loaded_lib_version);
        }
        else{
            // we have a different major version. we fail.
            warning("The PAPI version loaded at runtime differs greatly from the one DLB was compiled with. Expected version%d.%d.%d but got %d.%d.%d.",
                    PAPI_VERSION_MAJOR(PAPI_VER_CURRENT),
                    PAPI_VERSION_MINOR(PAPI_VER_CURRENT),
                    PAPI_VERSION_REVISION(PAPI_VER_CURRENT),
                    PAPI_VERSION_MAJOR(loaded_lib_version),
                    PAPI_VERSION_MINOR(loaded_lib_version),
                    PAPI_VERSION_REVISION(loaded_lib_version));
            return -1;
        }
    }
    
    if(retval < 0 || PAPI_is_initialized() == PAPI_NOT_INITED){
        // so we failed, we can maybe get some info why:
        if(retval == PAPI_ENOMEM){
            warning("PAPI initialization failed: Insufficient memory to complete the operation.");
        }
        if(retval == PAPI_ECMP){
            warning("PAPI initialization failed: This component does not support the underlying hardware.");
        }
        if(retval == PAPI_ESYS){
            warning("PAPI initialization failed: A system or C library call failed inside PAPI.");
        }
        return -1;
    }

    /* Activate thread tracing */
    int error = PAPI_thread_init(pthread_self);
    if (error != PAPI_OK) {
        warning("PAPI Error during thread initialization. %d: %s",
                error, PAPI_strerror(error));
        return -1;
    }
#endif
    return 0;
}

/* Called once per thread */
int talp_init_papi_counters(void) {
#ifdef PAPI_LIB
    ensure( ((talp_info_t*)thread_spd->talp_info)->flags.papi,
            "Error invoking %s when PAPI has been disabled", __FUNCTION__);

    int error = PAPI_register_thread();
    if (error != PAPI_OK) {
        warning("PAPI Error during thread registration. %d: %s",
                error, PAPI_strerror(error));
        return -1;
    }

    /* Eventset creation */
    EventSet = PAPI_NULL;
    error = PAPI_create_eventset(&EventSet);
    if (error != PAPI_OK) {
        warning("PAPI Error during eventset creation. %d: %s",
                error, PAPI_strerror(error));
        return -1;
    }

    int Events[2] = {PAPI_TOT_CYC, PAPI_TOT_INS};
    error = PAPI_add_events(EventSet, Events, 2);
    if (error != PAPI_OK) {
        warning("PAPI Error adding events. %d: %s",
                error, PAPI_strerror(error));
        return -1;
    }

    /* Start tracing  */
    error = PAPI_start(EventSet);
    if (error != PAPI_OK) {
        warning("PAPI Error during tracing initialization: %d: %s",
                error, PAPI_strerror(error));
        return -1;
    }
#endif
    return 0;
}

static inline void update_last_read_papi_counters(talp_sample_t* sample) {
#ifdef PAPI_LIB
    ensure( ((talp_info_t*)thread_spd->talp_info)->flags.papi,
            "Error invoking %s when PAPI has been disabled", __FUNCTION__);
    
    if(sample->state == useful) {
        // Similar to the old logic, we "reset" the PAPI counters here.
        // But instead of calling PAPI_Reset we just store the current value,
        // such that we can subtract the value later when updating the sample.
        long long papi_values[2];
        int error = PAPI_read(EventSet, papi_values);
        if (error != PAPI_OK) {
            verbose(VB_TALP, "Error reading PAPI counters: %d, %s", error, PAPI_strerror(error));
        }
        
        /* Update sample */
        sample->last_read_counters.cycles = papi_values[0];
        sample->last_read_counters.instructions = papi_values[1];
    }
#endif
}

/* Called once in the main thread */
static inline void fini_papi(void) __attribute__((unused));
static inline void fini_papi(void) {
#ifdef PAPI_LIB
    ensure( ((talp_info_t*)thread_spd->talp_info)->flags.papi,
            "Error invoking %s when PAPI has been disabled", __FUNCTION__);

    PAPI_shutdown();
#endif
}

/* Called once per thread */
void talp_fini_papi_counters(void) {
#ifdef PAPI_LIB
    ensure( ((talp_info_t*)thread_spd->talp_info)->flags.papi,
            "Error invoking %s when PAPI has been disabled", __FUNCTION__);

    int error;

    if (EventSet != PAPI_NULL) {
        // Stop counters (ignore if already stopped)
        error = PAPI_stop(EventSet, NULL);
        if (error != PAPI_OK && error != PAPI_ENOTRUN) {
            warning("PAPI Error stopping counters. %d: %s",
                    error, PAPI_strerror(error));
        }

        // Cleanup and destroy the EventSet
        error = PAPI_cleanup_eventset(EventSet);
        if (error != PAPI_OK) {
            warning("PAPI Error cleaning eventset. %d: %s",
                    error, PAPI_strerror(error));
        }

        error = PAPI_destroy_eventset(&EventSet);
        if (error != PAPI_OK) {
            warning("PAPI Error destroying eventset. %d: %s",
                    error, PAPI_strerror(error));
        }

        EventSet = PAPI_NULL;
    }

    // Unregister this thread from PAPI
    error = PAPI_unregister_thread();
    if (error != PAPI_OK) {
        warning("PAPI Error unregistering thread. %d: %s",
                error, PAPI_strerror(error));
    }
#endif
}


/*********************************************************************************/
/*    Init / Finalize                                                            */
/*********************************************************************************/

void talp_init(subprocess_descriptor_t *spd) {
    ensure(!spd->talp_info, "TALP already initialized");
    ensure(!thread_is_observer, "An observer thread cannot call talp_init");
    verbose(VB_TALP, "Initializing TALP module");

    /* Initialize talp info */
    talp_info_t *talp_info = malloc(sizeof(talp_info_t));
    *talp_info = (const talp_info_t) {
        .flags = {
            .external_profiler = spd->options.talp_external_profiler,
            .papi = spd->options.talp_papi,
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

    /* Initialize global region monitor
     * (at this point we don't know how many CPUs, it will be fixed in talp_openmp_init) */
    talp_info->monitor = region_register(spd, region_get_global_name());

    verbose(VB_TALP, "TALP module with workers mask: %s", mu_to_str(&spd->process_mask));

    /* Initialize and start running PAPI */
    if (talp_info->flags.papi) {
#ifdef PAPI_LIB
        int papi_local_fail = 0;
        int papi_global_fail = 0;

        if (init_papi() != 0 || talp_init_papi_counters() != 0) {
            papi_local_fail = 1;
        }

#ifdef MPI_LIB
        PMPI_Allreduce(&papi_local_fail, &papi_global_fail, 1, MPI_INT, MPI_MAX, getWorldComm());
#endif

        if (papi_global_fail && !papi_local_fail) {
            /* Un-initialize */
            talp_fini_papi_counters();
            fini_papi();
        }

        if (papi_global_fail || papi_local_fail) {
            warning0("PAPI initialization has failed, disabling option.");
            talp_info->flags.papi = false;
        }
#else
        warning0("DLB has not been configured with PAPI support, disabling option.");
        talp_info->flags.papi = false;
#endif
    }

    /* Start global region */
    region_start(spd, talp_info->monitor);
}

void talp_finalize(subprocess_descriptor_t *spd) {
    ensure(spd->talp_info, "TALP is not initialized");
    ensure(!thread_is_observer, "An observer thread cannot call talp_finalize");
    verbose(VB_TALP, "Finalizing TALP module");

    talp_info_t *talp_info = spd->talp_info;
    if (!talp_info->flags.have_mpi) {
        /* If we don't have MPI support, regions may be still running and
         * without being recorded to talp_output. Do that now. */

        /* Stop open regions
         * (Note that region_stop need to acquire the regions_mutex
         * lock, so we we need to iterate without it) */
        while(talp_info->open_regions != NULL) {
            dlb_monitor_t *monitor = talp_info->open_regions->data;
            region_stop(spd, monitor);
        }

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
    talp_output_finalize(spd->options.talp_output_file);

    /* Deallocate samples structure */
    talp_dealloc_samples(spd);

    /* Finalize shared memory */
    if (talp_info->flags.have_shmem || talp_info->flags.have_minimal_shmem) {
        shmem_talp__finalize(spd->id);
    }

#ifdef PAPI_LIB
    if (talp_info->flags.papi) {
        fini_papi();
    }
#endif

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

/* Quick test, without locking and without generating a new sample */
static inline bool is_talp_sample_mine(const talp_sample_t *sample) {
    return sample != NULL && sample == _tls_sample;
}

static void talp_dealloc_samples(const subprocess_descriptor_t *spd) {
    /* This only nullifies the current thread pointer, but this function is
     * only really important when TALP is finalized and then initialized again
     */
    _tls_sample = NULL;

    talp_info_t *talp_info = spd->talp_info;
    pthread_mutex_lock(&talp_info->samples_mutex);
    {
        free(talp_info->samples);
        talp_info->samples = NULL;
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

    talp_set_sample_state(_tls_sample, disabled, talp_info->flags.papi);

#ifdef INSTRUMENTATION_VERSION
    unsigned events[] = {MONITOR_CYCLES, MONITOR_INSTR};
    long long papi_values[] = {0, 0};
    instrument_nevent(2, events, papi_values);
#endif

    return _tls_sample;
}

/* WARNING: this function may only be called when updating own thread's sample */
void talp_set_sample_state(talp_sample_t *sample, enum talp_sample_state state,
        bool papi) {
    sample->state = state;
    if (papi) {
        update_last_read_papi_counters(sample);
    }
    instrument_event(MONITOR_STATE,
            state == disabled ? MONITOR_STATE_DISABLED
            : state == useful ? MONITOR_STATE_USEFUL
            : state == not_useful_mpi ? MONITOR_STATE_NOT_USEFUL_MPI
            : state == not_useful_omp_in ? MONITOR_STATE_NOT_USEFUL_OMP_IN
            : state == not_useful_omp_out ? MONITOR_STATE_NOT_USEFUL_OMP_OUT
            : state == not_useful_gpu ? MONITOR_STATE_NOT_USEFUL_GPU
            : 0,
            EVENT_BEGIN);
}

/* Compute new microsample (time since last update) and update sample values */
void talp_update_sample(talp_sample_t *sample, bool papi, int64_t timestamp) {
    /* Observer threads ignore this function */
    if (unlikely(sample == NULL)) return;

    /* Compute duration and set new last_updated_timestamp */
    int64_t now = timestamp == TALP_NO_TIMESTAMP ? get_time_in_ns() : timestamp;
    int64_t microsample_duration = now - sample->last_updated_timestamp;
    sample->last_updated_timestamp = now;

    /* Update the appropriate sample timer */
    switch(sample->state) {
        case disabled:
            break;
        case useful:
            DLB_ATOMIC_ADD_RLX(&sample->timers.useful, microsample_duration);
            break;
        case not_useful_mpi:
            DLB_ATOMIC_ADD_RLX(&sample->timers.not_useful_mpi, microsample_duration);
            break;
        case not_useful_omp_in:
            DLB_ATOMIC_ADD_RLX(&sample->timers.not_useful_omp_in, microsample_duration);
            break;
        case not_useful_omp_out:
            DLB_ATOMIC_ADD_RLX(&sample->timers.not_useful_omp_out, microsample_duration);
            break;
        case not_useful_gpu:
            DLB_ATOMIC_ADD_RLX(&sample->timers.not_useful_gpu, microsample_duration);
            break;
    }

#ifdef PAPI_LIB
    if (papi) {
        /* Only read counters if we are updating this thread's sample */
        if (is_talp_sample_mine(sample)) {
            if (sample->state == useful) {
                /* Read */
                long long papi_values[2];
                int error = PAPI_read(EventSet, papi_values);
                if (error != PAPI_OK) {
                    verbose(VB_TALP, "stop return code: %d, %s", error, PAPI_strerror(error));
                }
                // Compute the difference from the last value read
                const long long useful_cycles = papi_values[0] - sample->last_read_counters.cycles;
                const long long useful_instructions = papi_values[1] - sample->last_read_counters.instructions;

                /* Atomically add papi_values to sample structure */
                DLB_ATOMIC_ADD_RLX(&sample->counters.cycles, useful_cycles);
                DLB_ATOMIC_ADD_RLX(&sample->counters.instructions, useful_instructions);

#ifdef INSTRUMENTATION_VERSION
                unsigned events[] = {MONITOR_CYCLES, MONITOR_INSTR};
                instrument_nevent(2, events, papi_values);
#endif

                /* Counters are updated here and every time the sample is set to useful */
                sample->last_read_counters.cycles = papi_values[0];
                sample->last_read_counters.instructions = papi_values[1];
            }
            else {
#ifdef INSTRUMENTATION_VERSION
                /* Emit 0's to distinguish useful chunks in traces */
                unsigned events[] = {MONITOR_CYCLES, MONITOR_INSTR};
                long long papi_values[] = {0, 0};
                instrument_nevent(2, events, papi_values);
#endif
            }
        }
    }
#endif /* PAPI_LIB */
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

#ifdef PAPI_LIB
    /* Counters */
    macrosample->counters.cycles +=
        DLB_ATOMIC_EXCH_RLX(&sample->counters.cycles, 0);
    macrosample->counters.instructions +=
        DLB_ATOMIC_EXCH_RLX(&sample->counters.instructions, 0);
#endif

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

/* Flush values from gpu_sample to macrosample */
void flush_gpu_sample_to_macrosample(talp_gpu_sample_t *gpu_sample,
        talp_macrosample_t *macrosample) {

    macrosample->gpu_timers.useful        = gpu_sample->timers.useful;
    macrosample->gpu_timers.communication = gpu_sample->timers.communication;
    macrosample->gpu_timers.inactive      = gpu_sample->timers.inactive;

    /* Reset */
    *gpu_sample = (const talp_gpu_sample_t){0};
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
            talp_update_sample(talp_info->samples[i], talp_info->flags.papi, timestamp);
            flush_sample_to_macrosample(talp_info->samples[i], &macrosample);
        }
    }
    pthread_mutex_unlock(&talp_info->samples_mutex);

    if (talp_info->flags.have_gpu) {
        /* gpu_sample data is filled asynchronously, first we need to synchronize
         * and wait for the measurements to be read, then flush it */
        talp_gpu_sync_measurements();
        flush_gpu_sample_to_macrosample(&talp_info->gpu_sample, &macrosample);
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
            talp_update_sample(samples[i], talp_info->flags.papi, timestamp);
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
