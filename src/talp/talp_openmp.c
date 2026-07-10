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

#include "talp/talp_openmp.h"

#include "LB_numThreads/omptool.h"
#include "LB_comm/shmem_talp.h"
#include "LB_core/DLB_kernel.h"
#include "apis/dlb_talp.h"
#include "support/atomic.h"
#include "support/debug.h"
#include "support/small_array.h"
#include "talp/regions.h"
#include "talp/sample.h"
#include "talp/talp.h"
#include "talp/talp_hwc.h"
#include "talp/talp_types.h"

#include <unistd.h>

extern __thread bool thread_is_observer;
extern __thread bool thread_is_known;

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
                sample->last_updated_ts - monitor->start_time;
        }
    }
    pthread_mutex_unlock(&talp_info->regions_mutex);
}


static void compute_parallel_not_useful(
        talp_sample_t **parallel_samples,
        unsigned int num_samples,
        int64_t now,
        int64_t *not_useful_omp_in_lb,
        int64_t *not_useful_omp_in_sched) {

    int64_t min_not_useful_omp_in = INT64_MAX;

    SMALL_ARRAY(int64_t, workers_not_useful_omp_in, num_samples);

    /* Iterate first to compute the minimum not-useful-omp-in among all samples */
    for (unsigned int i = 0; i < num_samples; ++i) {
        // for each sample, we need to compute its time in not_useful_omp_in
        // for that we need to add each (now - sample->last_updated_ts)
        const talp_sample_t *worker_sample = parallel_samples[i];
        int64_t worker_not_useful_omp_in = worker_sample->timers.not_useful_omp_in +
            (now - worker_sample->last_updated_ts);
        min_not_useful_omp_in = min_int64(min_not_useful_omp_in, worker_not_useful_omp_in);
        workers_not_useful_omp_in[i] = worker_not_useful_omp_in;
    }

    int64_t sched_timer = min_not_useful_omp_in * num_samples;
    int64_t lb_timer = 0;

    /* Iterate again to accumulate Load Balance */
    for (unsigned int i = 0; i < num_samples; ++i) {
        lb_timer += workers_not_useful_omp_in[i] - min_not_useful_omp_in;
    }

    *not_useful_omp_in_lb = lb_timer;
    *not_useful_omp_in_sched = sched_timer;
}


/*********************************************************************************/
/*    TALP OpenMP functions                                                      */
/*********************************************************************************/

/* Private TALP parallel data:
 *   For level 1, static storage for the struct, and parallel_samples realloc'ing if needed
 *   For other levels, struct is dynamically allocated */
typedef struct talp_parallel_data_t {
    talp_sample_t**  parallel_samples;
    int64_t          previous_not_useful_omp_in;
} talp_parallel_data_t;

static talp_parallel_data_t talp_parallel_data_l1 = {0};
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
        talp_sample_set_state(talp_info, TALP_STATE_USEFUL);
    }
}

void talp_openmp_finalize(void) {
    if (talp_parallel_data_l1.parallel_samples != NULL) {
        free(talp_parallel_data_l1.parallel_samples);
        talp_parallel_data_l1.parallel_samples = NULL;
        parallel_samples_l1_capacity = 0;
    }
}

// native-thread-begin event
void talp_openmp_thread_begin(ompt_thread_t thread_type) {

    const subprocess_descriptor_t *spd = thread_spd;
    talp_info_t *talp_info = spd->talp_info;

    if (talp_info == NULL || !talp_info->flags.have_openmp) return;

    /* OpenMP threads are known threads */
    thread_is_known = true;

    /* Initial thread is already in useful state, set omp_out for others */
    talp_sample_t *sample = talp_sample_get(talp_info);
    if (sample->state == TALP_STATE_DISABLED) {
        /* Not initial thread: */
        if (talp_info->flags.have_hwc) {
            talp_hwc_thread_init();
        }
        talp_sample_set_state(talp_info, TALP_STATE_NOT_USEFUL_OMP_OUT);

        /* The initial time of the sample is set to match the start time of
            * the innermost open region, but other nested open regions need to
            * be fixed */
        update_serialization_in_nested_regions(spd, sample);
    }
}

// native-thread-end event
void talp_openmp_thread_end(void) {

    const subprocess_descriptor_t *spd = thread_spd;
    talp_info_t *talp_info = spd->talp_info;

    if (talp_info == NULL || !talp_info->flags.have_openmp) return;

    /* Update thread sample */
    talp_sample_update(talp_info);

    /* Update state */
    talp_sample_set_state(talp_info, TALP_STATE_DISABLED);

    /* Finalize PAPI per-thread state */
    if (talp_info->flags.have_hwc) {
        talp_hwc_thread_finalize();
    }
}

// parallel-begin event
void talp_openmp_parallel_begin(omptool_parallel_data_t *parallel_data) {

    fatal_cond(parallel_data->requested_parallelism < 1,
            "Requested parallel region of invalid size in %s. Please report bug at %s.",
            __func__, PACKAGE_BUGREPORT);

    const subprocess_descriptor_t *spd = thread_spd;
    talp_info_t *talp_info = spd->talp_info;

    if (talp_info == NULL || !talp_info->flags.have_openmp) return;

    int parallel_level = parallel_data->level;

    if (parallel_level == 1) {
        /* Resize samples of parallel 1 if needed */
        unsigned int requested_parallelism = parallel_data->requested_parallelism;
        if (requested_parallelism > parallel_samples_l1_capacity) {
            void *ptr = realloc(talp_parallel_data_l1.parallel_samples,
                    sizeof(talp_sample_t*)*requested_parallelism);
            fatal_cond(!ptr, "realloc failed in %s", __func__);
            talp_parallel_data_l1.parallel_samples = ptr;
            parallel_samples_l1_capacity = requested_parallelism;
        }

        /* Assign local data */
        parallel_data->talp_parallel_data = &talp_parallel_data_l1;

    } else if (parallel_level > 1) {
        /* Allocate parallel samples array */
        unsigned int requested_parallelism = parallel_data->requested_parallelism;
        talp_parallel_data_t *talp_parallel_data = malloc(sizeof(talp_parallel_data_t));
        fatal_cond(!talp_parallel_data, "malloc failed in %s", __func__);
        *talp_parallel_data = (talp_parallel_data_t) {
            .parallel_samples = malloc(sizeof(talp_sample_t*)*requested_parallelism),
        };
        fatal_cond(!talp_parallel_data->parallel_samples, "malloc failed in %s", __func__);

        /* Assign local data */
        parallel_data->talp_parallel_data = talp_parallel_data;
    }

    /* Update stats */
    talp_sample_t *sample = talp_sample_get(talp_info);
    ++sample->stats.num_omp_parallels;

    /* Update main thread sequential mode if this is the outermost parallel region */
    if (parallel_level == 1) {
        talp_sample_set_main_sequential(false);
    }
}

// parallel-end event
void talp_openmp_parallel_end(omptool_parallel_data_t *parallel_data) {

    const subprocess_descriptor_t *spd = thread_spd;
    talp_info_t *talp_info = spd->talp_info;

    if (talp_info == NULL || !talp_info->flags.have_openmp) return;

    /* Update thread */
    talp_sample_update(talp_info);
    talp_sample_t *sample = talp_sample_get(talp_info);
    int64_t now = sample->last_updated_ts;

    /* Compute not_useful_omp_in_lb and not_useful_omp_in_sched for this
     * parallel region. The primary thread reads samples from all participant
     * threads and writes the computed timings into its sample. */
    talp_parallel_data_t *talp_parallel_data = parallel_data->talp_parallel_data;
    talp_sample_t **parallel_samples = talp_parallel_data->parallel_samples;
    unsigned int num_samples = parallel_data->actual_parallelism;
    int64_t not_useful_omp_in_lb;
    int64_t not_useful_omp_in_sched;
    compute_parallel_not_useful(parallel_samples, num_samples, now,
            &not_useful_omp_in_lb, &not_useful_omp_in_sched);

    sample->timers.not_useful_omp_in_lb += not_useful_omp_in_lb;
    sample->timers.not_useful_omp_in_sched += not_useful_omp_in_sched;

    /* Iterate all participants' samples and record the timestamp of the parallel-end event.
     * Note that this is needed because we don't know if the OpenMP implementation
     * wakes up worker threads to notify the implicit-task-end event in time. */
    for (unsigned int i = 1; i < num_samples; ++i) {
        talp_sample_t *worker_sample = parallel_samples[i];
        DLB_ATOMIC_ST_RLX(&worker_sample->last_parallel_end_ts, now);
    }

    /* Update current thread's state */
    talp_sample_set_state(talp_info, TALP_STATE_USEFUL);

    if (parallel_data->level == 1) {
        /* Update main thread sequential mode if this was the outermost parallel region */
        talp_sample_set_main_sequential(true);
    } else {
        /* Restore previously pushed not-useful-omp-in */
        sample->timers.not_useful_omp_in = talp_parallel_data->previous_not_useful_omp_in;

        /* free local data */
        free(talp_parallel_data);
        parallel_data->talp_parallel_data = NULL;
    }
}

// implicit-task-begin event
void talp_openmp_into_parallel_function(
        omptool_parallel_data_t *parallel_data, unsigned int index) {

    const subprocess_descriptor_t *spd = thread_spd;
    talp_info_t *talp_info = spd->talp_info;

    if (talp_info == NULL || !talp_info->flags.have_openmp) return;

    /* Assign thread sample as team-worker of this parallel */
    talp_parallel_data_t *talp_parallel_data = parallel_data->talp_parallel_data;
    talp_sample_t *sample = talp_sample_get(talp_info);
    talp_sample_t **parallel_samples = talp_parallel_data->parallel_samples;
    /* Probably optimized, but try to avoid invalidating
        * the cache line on reused parallel data */
    if (parallel_samples[index] != sample) {
        parallel_samples[index] = sample;
    }

    if (index > 0) {
        /* For non-primary threads, the next sample update will add time to
         * not-useful-omp-out, but we need to fix the initial timestamp to not
         * count the not-useful-omp-in already aggregated by the initial thread. */
        int64_t last_parallel_end_ts = DLB_ATOMIC_LD_RLX(&sample->last_parallel_end_ts);
        if (sample->last_updated_ts < last_parallel_end_ts) {
            sample->last_updated_ts = last_parallel_end_ts;
        }

    } else if (parallel_data->level > 1) {
        /* For primary threads on nested regions, we need to save the accumulated time
         * in not_useful_omp_in that belongs to the previous parallel region. */
        talp_parallel_data->previous_not_useful_omp_in = sample->timers.not_useful_omp_in;
    }

    /* We always start parallel regions with this timer reset to 0. */
    sample->timers.not_useful_omp_in = 0;

    /* Update thread sample */
    talp_sample_update(talp_info);

    /* Update state */
    talp_sample_set_state(talp_info, TALP_STATE_USEFUL);
}

// implicit-task-end event (thread 0 skips this function)
void talp_openmp_outof_parallel_function(void) {

    const subprocess_descriptor_t *spd = thread_spd;
    talp_info_t *talp_info = spd->talp_info;

    if (talp_info == NULL || !talp_info->flags.have_openmp) return;

    /* Ideally, we should be updating the microsample at this point:
     *   not_useful_omp_in += now - last_updated_ts
     *
     * But, we don't know if the OpemMP implementation is calling this function
     * on time, so the primary thread will be the responsible to reconstruct
     * the missing not_useful_omp_in from us in the parallel-end event.
     *
     * Since measuring from this function is not consistent, we'll just change
     * the state to correctly compute not-useful-omp-out later, and to add an
     * instrumentation event. HWC readings should also be skipped since we are
     * changing not_useful -> not_useful anyway. */

    /* Update state */
    talp_sample_set_state(talp_info, TALP_STATE_NOT_USEFUL_OMP_OUT);
}

// implicit-barrier-begin event (sync_region_barrier_implicit_parallel)
void talp_openmp_into_parallel_implicit_barrier(omptool_parallel_data_t *parallel_data) {

    const subprocess_descriptor_t *spd = thread_spd;
    talp_info_t *talp_info = spd->talp_info;

    if (talp_info == NULL || !talp_info->flags.have_openmp) return;

    /* Update thread sample */
    talp_sample_update(talp_info);

    /* Update state */
    talp_sample_set_state(talp_info, TALP_STATE_NOT_USEFUL_OMP_IN);
}

// {*-barrier,taskwait,taskgroup}-begin event
void talp_openmp_into_parallel_sync(omptool_parallel_data_t *parallel_data) {

    const subprocess_descriptor_t *spd = thread_spd;
    talp_info_t *talp_info = spd->talp_info;

    if (talp_info == NULL || !talp_info->flags.have_openmp) return;

    /* Update thread sample */
    talp_sample_update(talp_info);

    /* Update state */
    talp_sample_set_state(talp_info, TALP_STATE_NOT_USEFUL_OMP_IN);
}

// {*-barrier,taskwait,taskgroup}-end event
void talp_openmp_outof_parallel_sync(omptool_parallel_data_t *parallel_data) {

    const subprocess_descriptor_t *spd = thread_spd;
    talp_info_t *talp_info = spd->talp_info;

    if (talp_info == NULL || !talp_info->flags.have_openmp) return;

    /* Update thread sample */
    talp_sample_update(talp_info);

    /* Update state */
    talp_sample_set_state(talp_info, TALP_STATE_USEFUL);
}

// task-create event
void talp_openmp_task_create(void) {

    const subprocess_descriptor_t *spd = thread_spd;
    talp_info_t *talp_info = spd->talp_info;

    if (talp_info == NULL || !talp_info->flags.have_openmp) return;

    /* Just update stats */
    talp_sample_t *sample = talp_sample_get(talp_info);
    ++sample->stats.num_omp_tasks;
}

// task-schedule event: task complete
void talp_openmp_task_complete(void) {

    const subprocess_descriptor_t *spd = thread_spd;
    talp_info_t *talp_info = spd->talp_info;

    if (talp_info == NULL || !talp_info->flags.have_openmp) return;

    /* Update thread sample */
    talp_sample_update(talp_info);

    /* Update state (FIXME: tasks outside of parallels?) */
    talp_sample_set_state(talp_info, TALP_STATE_NOT_USEFUL_OMP_IN);
}

// task-schedule event: task switch
void talp_openmp_task_switch(void) {

    const subprocess_descriptor_t *spd = thread_spd;
    talp_info_t *talp_info = spd->talp_info;

    if (talp_info == NULL || !talp_info->flags.have_openmp) return;

    /* Update thread sample */
    talp_sample_update(talp_info);

    /* Update state */
    talp_sample_set_state(talp_info, TALP_STATE_USEFUL);
}


/*********************************************************************************/
/*  Vtable for handling omptool events                                           */
/*********************************************************************************/

const omptool_event_funcs_t talp_events_vtable = (const omptool_event_funcs_t) {
    .init                           = talp_openmp_init,
    .finalize                       = talp_openmp_finalize,
    .into_mpi                       = NULL,
    .outof_mpi                      = NULL,
    .lend_from_api                  = NULL,
    .thread_begin                   = talp_openmp_thread_begin,
    .thread_end                     = talp_openmp_thread_end,
    .thread_role_shift              = NULL,
    .parallel_begin                 = talp_openmp_parallel_begin,
    .parallel_end                   = talp_openmp_parallel_end,
    .into_parallel_function         = talp_openmp_into_parallel_function,
    .outof_parallel_function        = talp_openmp_outof_parallel_function,
    .into_parallel_implicit_barrier = talp_openmp_into_parallel_implicit_barrier,
    .into_parallel_sync             = talp_openmp_into_parallel_sync,
    .outof_parallel_sync            = talp_openmp_outof_parallel_sync,
    .task_create                    = talp_openmp_task_create,
    .task_complete                  = talp_openmp_task_complete,
    .task_switch                    = talp_openmp_task_switch,
};
