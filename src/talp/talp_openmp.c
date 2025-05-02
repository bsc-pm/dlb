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

#include "talp/talp_openmp.h"

#include "LB_numThreads/omptool.h"
#include "LB_comm/shmem_talp.h"
#include "LB_core/DLB_kernel.h"
#include "apis/dlb_talp.h"
#include "support/debug.h"
#include "talp/regions.h"
#include "talp/talp.h"
#include "talp/talp_types.h"

#include <unistd.h>

extern __thread bool thread_is_observer;


/* Update all open nested regions (so, excluding the innermost) and add the
 * time since its start time until the sample last timestamp (which is the time
 * that has yet not been added to the regions) as omp_serialization_time */
static void update_serialization_in_nested_regions(const subprocess_descriptor_t *spd,
        const talp_sample_t *sample) {

    talp_info_t *talp_info = spd->talp_info;

    /* Update all open nested regions */
    pthread_mutex_lock(&talp_info->regions_mutex);
    {
        GSList *nested_open_regions = talp_info->open_regions
            ? talp_info->open_regions->next
            : NULL;

        for (GSList *node = nested_open_regions;
                node != NULL;
                node = node->next) {

            dlb_monitor_t *monitor = node->data;
            monitor->omp_serialization_time +=
                sample->last_updated_timestamp - monitor->start_time;
        }
    }
    pthread_mutex_unlock(&talp_info->regions_mutex);
}


/*********************************************************************************/
/*    TALP OpenMP functions                                                      */
/*********************************************************************************/

/* samples involved in parallel level 1 */
static talp_sample_t** parallel_samples_l1 = NULL;
static unsigned int parallel_samples_l1_capacity = 0;

void talp_openmp_init(pid_t pid, const options_t* options) {
    ensure(!thread_is_observer, "An observer thread cannot call talp_openmp_init");

    const subprocess_descriptor_t *spd = thread_spd;
    talp_info_t *talp_info = spd->talp_info;
    if (talp_info) {
        monitor_data_t *monitor_data = talp_info->monitor->_data;
        talp_info->flags.have_openmp = true;

        /* Fix up number of CPUs for the global region */
        float cpus = CPU_COUNT(&spd->process_mask);
        talp_info->monitor->avg_cpus = cpus;
        shmem_talp__set_avg_cpus(monitor_data->node_shared_id, cpus);

        /* Start global region (no-op if already started) */
        region_start(spd, talp_info->monitor);

        /* Set useful state */
        talp_sample_t *sample = talp_get_thread_sample(spd);
        talp_set_sample_state(sample, useful, talp_info->flags.papi);
    }
}

void talp_openmp_finalize(void) {
    if (parallel_samples_l1 != NULL) {
        free(parallel_samples_l1);
        parallel_samples_l1 = NULL;
        parallel_samples_l1_capacity = 0;
    }
}

void talp_openmp_thread_begin() {
    const subprocess_descriptor_t *spd = thread_spd;
    talp_info_t *talp_info = spd->talp_info;
    if (talp_info) {
        /* Initial thread is already in useful state, set omp_out for others */
        talp_sample_t *sample = talp_get_thread_sample(spd);
        if (sample->state == disabled) {
            /* Not initial thread: */
            if (talp_info->flags.papi) {
                talp_init_papi_counters();
            }
            talp_set_sample_state(sample, not_useful_omp_out, talp_info->flags.papi);

            /* The initial time of the sample is set to match the start time of
             * the innermost open region, but other nested open regions need to
             * be fixed */
            update_serialization_in_nested_regions(spd, sample);
        }
    }
}

void talp_openmp_thread_end(void) {
    const subprocess_descriptor_t *spd = thread_spd;
    talp_info_t *talp_info = spd->talp_info;
    if (talp_info) {
        /* Update thread sample with the last microsample */
        talp_sample_t *sample = talp_get_thread_sample(spd);
        talp_update_sample(sample, talp_info->flags.papi, TALP_NO_TIMESTAMP);

        /* Update state */
        talp_set_sample_state(sample, disabled, talp_info->flags.papi);
    }
}

void talp_openmp_parallel_begin(omptool_parallel_data_t *parallel_data) {
    fatal_cond(parallel_data->requested_parallelism < 1,
            "Requested parallel region of invalid size in %s. Please report bug at %s.",
            __func__, PACKAGE_BUGREPORT);

    const subprocess_descriptor_t *spd = thread_spd;
    talp_info_t *talp_info = spd->talp_info;
    if (talp_info) {
        if (parallel_data->level == 1) {
            /* Resize samples of parallel 1 if needed */
            unsigned int requested_parallelism = parallel_data->requested_parallelism;
            if (requested_parallelism > parallel_samples_l1_capacity) {
                void *ptr = realloc(parallel_samples_l1,
                        sizeof(talp_sample_t*)*requested_parallelism);
                fatal_cond(!ptr, "realloc failed in %s", __func__);
                parallel_samples_l1 = ptr;
                parallel_samples_l1_capacity = requested_parallelism;
            }

            /* Assign local data */
            parallel_data->talp_parallel_data = parallel_samples_l1;

        } else if (parallel_data->level > 1) {
            /* Allocate parallel samples array */
            unsigned int requested_parallelism = parallel_data->requested_parallelism;
            void *ptr = malloc(sizeof(talp_sample_t*)*requested_parallelism);
            fatal_cond(!ptr, "malloc failed in %s", __func__);

            /* Assign local data */
            parallel_data->talp_parallel_data = ptr;
        }

        /* Update stats */
        talp_sample_t *sample = talp_get_thread_sample(spd);
        DLB_ATOMIC_ADD_RLX(&sample->stats.num_omp_parallels, 1);
    }
}

void talp_openmp_parallel_end(omptool_parallel_data_t *parallel_data) {
    const subprocess_descriptor_t *spd = thread_spd;
    talp_info_t *talp_info = spd->talp_info;
    if (talp_info) {
        /* Update thread sample with the last microsample */
        talp_sample_t *sample = talp_get_thread_sample(spd);
        talp_update_sample(sample, talp_info->flags.papi, TALP_NO_TIMESTAMP);

        if (parallel_data->level == 1) {
            /* Flush and aggregate all samples of the parallel region */
            talp_flush_sample_subset_to_regions(spd,
                    parallel_data->talp_parallel_data,
                    parallel_data->actual_parallelism);

        } else if (parallel_data->level > 1) {
            /* Flush and aggregate all samples of this parallel except this
             * thread's sample. The primary thread of a nested parallel region
             * will keep its samples until it finishes as non-primary
             * team-worker or reaches the level 1 parallel region */
            talp_sample_t **parallel_samples = parallel_data->talp_parallel_data;
            talp_flush_sample_subset_to_regions(spd,
                    &parallel_samples[1],
                    parallel_data->actual_parallelism-1);

            /* free local data */
            free(parallel_data->talp_parallel_data);
            parallel_data->talp_parallel_data = NULL;
        }

        /* Update current threads's state */
        talp_set_sample_state(sample, useful, talp_info->flags.papi);

        /* Update the state of the rest of team-worker threads
         * (note that talp_set_sample_state cannot be used here because we are
         * impersonating a worker thread) */
        talp_sample_t **parallel_samples = parallel_data->talp_parallel_data;
        for (unsigned int i = 1; i < parallel_data->actual_parallelism; ++i) {
            talp_sample_t *worker_sample = parallel_samples[i];
            if (worker_sample->state == not_useful_omp_in) {
                worker_sample->state = not_useful_omp_out;
            }
        }
    }
}

void talp_openmp_into_parallel_function(
        omptool_parallel_data_t *parallel_data, unsigned int index) {
    const subprocess_descriptor_t *spd = thread_spd;
    talp_info_t *talp_info = spd->talp_info;
    if (talp_info) {
        /* Assign thread sample as team-worker of this parallel */
        talp_sample_t *sample = talp_get_thread_sample(spd);
        talp_sample_t **parallel_samples = parallel_data->talp_parallel_data;
        /* Probably optimized, but try to avoid invalidating
         * the cache line on reused parallel data */
        if (parallel_samples[index] != sample) {
            parallel_samples[index] = sample;
        }

        /* Update thread sample with the last microsample */
        talp_update_sample(sample, talp_info->flags.papi, TALP_NO_TIMESTAMP);

        /* Update state */
        talp_set_sample_state(sample, useful, talp_info->flags.papi);
    }
}

void talp_openmp_outof_parallel_function(void) {
    const subprocess_descriptor_t *spd = thread_spd;
    talp_info_t *talp_info = spd->talp_info;
    if (talp_info) {
        /* Update thread sample with the last microsample */
        talp_sample_t *sample = talp_get_thread_sample(spd);
        talp_update_sample(sample, talp_info->flags.papi, TALP_NO_TIMESTAMP);

        /* Update state */
        talp_set_sample_state(sample, not_useful_omp_out, talp_info->flags.papi);
    }
}

void talp_openmp_into_parallel_implicit_barrier(omptool_parallel_data_t *parallel_data) {
    const subprocess_descriptor_t *spd = thread_spd;
    talp_info_t *talp_info = spd->talp_info;
    if (talp_info) {
        /* Update thread sample with the last microsample */
        talp_sample_t *sample = talp_get_thread_sample(spd);
        talp_update_sample(sample, talp_info->flags.papi, TALP_NO_TIMESTAMP);

        /* Update state */
        talp_set_sample_state(sample, not_useful_omp_in, talp_info->flags.papi);
    }
}

void talp_openmp_into_parallel_sync(omptool_parallel_data_t *parallel_data) {
    const subprocess_descriptor_t *spd = thread_spd;
    talp_info_t *talp_info = spd->talp_info;
    if (talp_info) {
        /* Update thread sample with the last microsample */
        talp_sample_t *sample = talp_get_thread_sample(spd);
        talp_update_sample(sample, talp_info->flags.papi, TALP_NO_TIMESTAMP);

        /* Update state */
        talp_set_sample_state(sample, not_useful_omp_in, talp_info->flags.papi);
    }
}

void talp_openmp_outof_parallel_sync(omptool_parallel_data_t *parallel_data) {
    const subprocess_descriptor_t *spd = thread_spd;
    talp_info_t *talp_info = spd->talp_info;
    if (talp_info) {
        /* Update thread sample with the last microsample */
        talp_sample_t *sample = talp_get_thread_sample(spd);
        talp_update_sample(sample, talp_info->flags.papi, TALP_NO_TIMESTAMP);

        /* Update state */
        talp_set_sample_state(sample, useful, talp_info->flags.papi);
    }
}

void talp_openmp_task_create(void) {
    const subprocess_descriptor_t *spd = thread_spd;
    talp_info_t *talp_info = spd->talp_info;
    if (talp_info) {
        /* Just update stats */
        talp_sample_t *sample = talp_get_thread_sample(spd);
        DLB_ATOMIC_ADD_RLX(&sample->stats.num_omp_tasks, 1);
    }
}

void talp_openmp_task_complete(void) {
    const subprocess_descriptor_t *spd = thread_spd;
    talp_info_t *talp_info = spd->talp_info;
    if (talp_info) {
        /* Update thread sample with the last microsample */
        talp_sample_t *sample = talp_get_thread_sample(spd);
        talp_update_sample(sample, talp_info->flags.papi, TALP_NO_TIMESTAMP);

        /* Update state (FIXME: tasks outside of parallels? */
        talp_set_sample_state(sample, not_useful_omp_in, talp_info->flags.papi);
    }
}

void talp_openmp_task_switch(void) {
    const subprocess_descriptor_t *spd = thread_spd;
    talp_info_t *talp_info = spd->talp_info;
    if (talp_info) {
        /* Update thread sample with the last microsample */
        talp_sample_t *sample = talp_get_thread_sample(spd);
        talp_update_sample(sample, talp_info->flags.papi, TALP_NO_TIMESTAMP);

        /* Update state */
        talp_set_sample_state(sample, useful, talp_info->flags.papi);
    }
}
