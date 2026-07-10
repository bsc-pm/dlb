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
#include "LB_core/thread_ctx.h"
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
#include "talp/sample.h"
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
    ensure(!thread_is_observer(), "An observer thread cannot call talp_init");
    verbose(VB_TALP, "Initializing TALP module with worker mask: %s",
            mu_to_str(&spd->process_mask));

    int num_cpus = CPU_COUNT(&spd->process_mask);
    if (num_cpus == 0) {
        /* Possibly due to testing or DLB_Init, query process mask: */
        cpu_set_t process_mask;
        sched_getaffinity(0, sizeof(process_mask), &process_mask);
        num_cpus = CPU_COUNT(&process_mask);
    }

    /* Initialize talp info */
    talp_info_t *talp_info = malloc(sizeof(talp_info_t));
    *talp_info = (const talp_info_t) {
        .flags = {
            .external_profiler = spd->options.talp_external_profiler,
            .have_shmem = spd->options.talp_external_profiler,
            .have_minimal_shmem = !spd->options.talp_external_profiler
                && spd->options.talp_summary & SUMMARY_NODE,
        },
        .num_cpus = num_cpus,
        .regions = g_tree_new_full(
                (GCompareDataFunc)region_compare_by_name,
                NULL, NULL, region_dealloc),
        .regions_mutex = PTHREAD_MUTEX_INITIALIZER,
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

    /* Initialize sample structure */
    talp_sample_init(talp_info);

    /* Create the main thread's sample */
    (void)talp_sample_get(talp_info);
    talp_sample_record_cpuid(talp_info);
    talp_sample_set_state(talp_info, TALP_STATE_USEFUL);

    /* Initialize global region monitor
     * (at this point we don't know how many CPUs, it will be fixed in talp_openmp_init) */
    talp_info->monitor = region_register(spd, region_get_global_name());

    /* Start global region */
    region_start(spd, talp_info->monitor);
}

void talp_finalize(subprocess_descriptor_t *spd) {
    ensure(spd->talp_info, "TALP is not initialized");
    ensure(thread_is_main(), "Only main thread can call talp_finalize");
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
    talp_sample_finalize(talp_info);

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
/*    Functions for sample aggregation to regions                                */
/*********************************************************************************/

void talp_aggregate_sample_to_region(talp_info_t *talp_info,
        dlb_monitor_t *monitor, const talp_sample_t *sample, int64_t elapsed) {

    pthread_mutex_lock(&talp_info->regions_mutex);
    {
        monitor_data_t *monitor_data = monitor->_data;

        /* CPU mask */
        CPU_OR(&monitor_data->cpu_mask, &monitor_data->cpu_mask, &sample->cpu_mask);

        /* Number of CPUs */
        monitor->num_cpus = min_int(CPU_COUNT(&monitor_data->cpu_mask), talp_info->num_cpus);

        /* Number of threads: we oversimplify here, and sometimes we may give
         * a wrong number. The solution would be to keep track of thread ids
         * that have participated. Not worth the effort. */
        monitor->num_omp_threads = monitor->num_cpus;

        /* Timers */
        /* Note: not_useful_omp_{during_mpi,lb,sched} are purposely not reduced
         * and the entire omp_in is attributed to omp_scheduling. */
        monitor->useful_time            += sample->timers.useful;
        monitor->mpi_time               += sample->timers.not_useful_mpi;
        monitor->omp_scheduling_time    += sample->timers.not_useful_omp_in;
        monitor->gpu_runtime_time       += sample->timers.not_useful_gpu;

        /* Counters */
        monitor->cycles                 += sample->counters.cycles;
        monitor->instructions           += sample->counters.instructions;

        /* Stats */
        monitor->num_mpi_calls          += sample->stats.num_mpi_calls;
        monitor->num_omp_parallels      += sample->stats.num_omp_parallels;
        monitor->num_omp_tasks          += sample->stats.num_omp_tasks;
        monitor->num_gpu_runtime_calls  += sample->stats.num_gpu_runtime_calls;

        monitor->elapsed_time += elapsed;
        ++(monitor->num_measurements);
    }
    pthread_mutex_unlock(&talp_info->regions_mutex);
}

/* Update all open regions with the macrosample */
static void update_regions_with_macrosample(talp_info_t *restrict talp_info,
        const talp_macrosample_t *restrict macrosample) {

    /* Update all open regions */
    pthread_mutex_lock(&talp_info->regions_mutex);
    {
        for (GSList *node = talp_info->open_regions;
                node != NULL;
                node = node->next) {
            dlb_monitor_t *monitor = node->data;
            monitor_data_t *monitor_data = monitor->_data;

            /* CPU mask */
            CPU_OR(&monitor_data->cpu_mask, &monitor_data->cpu_mask, &macrosample->cpu_mask);

            /* Number of CPUs:
             * - the CPU mask in macrosample may have more CPUs than the actual
             *   number of threads, e.g.: when OMP_PLACES=cores or when the initial
             *   thread is bound only after the first parallel region.
             * -  */
            int macrosample_num_cpus = min_int(
                    CPU_COUNT(&macrosample->cpu_mask), macrosample->num_samples);
            if (monitor->num_cpus < macrosample_num_cpus) {
                monitor->num_cpus = macrosample_num_cpus;
            }
            ensure(monitor->num_cpus > 0, "Updating region with 0 CPUs. Please report.");

            /* if (monitor == talp_info->monitor) { */
            /*     warning("Macrosample CPUs: %s", mu_to_str(&macrosample->cpu_mask)); */
            /*     warning("Region      CPUs: %s", mu_to_str(&monitor_data->cpu_mask)); */
            /* } */

            /* Number of threads */
            if (monitor->num_omp_threads < macrosample->num_samples) {
                monitor->num_omp_threads = macrosample->num_samples;
            }

            /* Timers */
            monitor->useful_time             += macrosample->timers.useful;
            monitor->mpi_time                += macrosample->timers.not_useful_mpi;
            monitor->mpi_worker_idle_time    += macrosample->timers.not_useful_omp_during_mpi;
            monitor->omp_load_imbalance_time += macrosample->timers.not_useful_omp_in_lb;
            monitor->omp_scheduling_time     += macrosample->timers.not_useful_omp_in_sched;
            monitor->omp_serialization_time  += macrosample->timers.not_useful_omp_out;
            monitor->gpu_runtime_time        += macrosample->timers.not_useful_gpu;

            /* GPU Timers */
            monitor->gpu_useful_time         += macrosample->gpu_timers.useful;
            monitor->gpu_communication_time  += macrosample->gpu_timers.communication;
            monitor->gpu_inactive_time       += macrosample->gpu_timers.inactive;

            /* Counters */
            monitor->cycles                  += macrosample->counters.cycles;
            monitor->instructions            += macrosample->counters.instructions;

            /* Stats */
            monitor->num_mpi_calls           += macrosample->stats.num_mpi_calls;
            monitor->num_omp_parallels       += macrosample->stats.num_omp_parallels;
            monitor->num_omp_tasks           += macrosample->stats.num_omp_tasks;
            monitor->num_gpu_runtime_calls   += macrosample->stats.num_gpu_runtime_calls;

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

/* Accumulate values from samples of all threads and update regions */
int talp_aggregate_samples_to_regions(talp_info_t *talp_info) {

    /* Observer and unknown threads can't aggregate samples */
    if (unlikely(!thread_is_profiled())) return DLB_ERR_PERM;

    /* Accumulate samples from all threads */
    talp_macrosample_t macrosample = {0};
    talp_sample_aggregate_all_to_macrosample(talp_info, &macrosample);

    if (talp_info->flags.have_gpu) {
        /* Collect GPU measuremnts up to this point and update macrosample */
        gpu_measurements_t measurements;
        talp_gpu_collect(&measurements);
        macrosample.gpu_timers.useful        = measurements.useful_time;
        macrosample.gpu_timers.communication = measurements.communication_time;
        macrosample.gpu_timers.inactive      = measurements.inactive_time;
    }

    /* Update all started regions */
    update_regions_with_macrosample(talp_info, &macrosample);

    return DLB_SUCCESS;
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
