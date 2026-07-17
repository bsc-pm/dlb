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

/*
 * Here we describe the logic of the DLB module MNGO.
 *
 * The concept of MNGO is that it is in charge of periodically sampling the
 * applications performance and take action acordingly by
 * activating/deactivating other other DLB modules to improve the overall
 * applications efficiency.
 *
 * The general idea is to use TALP to collect different metrics (both within
 * the same node, and across). Then use these metrics to decide wheather or not
 * activating LeWI or DROM would improve the applications performance or
 * efficiency, and activate the corresponding module based on different
 * polices.
 */

/*
 * For activating the DLB modules in a coordinated fashion we have implemented a
 * shared memory interface.
 */
#include "LB_comm/shmem_barrier.h"
#include "LB_comm/shmem_mngo.h"

/*
 * To use DROM we use the procinfo shared memory interface to change the process
 * masks according to polices.
 */
#include "LB_comm/shmem_procinfo.h"

/*
 * To activate and deactivate LeWI we use the `set_lewi_enabled` function
 * included in the DLB_kernel.h
 */
#include "LB_core/DLB_kernel.h"

// Used to set the thread to observer when using the helper-thread mode
#include "LB_core/thread_ctx.h"

/*
 * The MODULE communicates with TALP to gather performance metrics of the
 * application.
 */
#include "LB_core/node_barrier.h"
#include "LB_core/thread_ctx.h"
#include "apis/dlb_mngo.h"
#include "dlb_talp.h"
#include "mngo/mngo_drom.h"
#include "mngo/mngo_resources.h"
#include "talp/regions.h"
#include "talp/talp.h"
/*
 *
 */
#include "LB_core/spd.h"
/*
 *
 */
#include "mngo/mngo.h"
#include "mngo/mngo_balancer.h"
/*
 * All the functions where something could go wrong will report an error defined
 * as an enum value in DLBErrorCodes.
 */
#include "apis/dlb_errors.h"
/*
 * We have used the verbose function to report extra information about what the
 * MODULE is doing when the debug flag is enabled.
 */
#include "support/debug.h"
/*
 * Similarly we use the Extrae API to generate MNGO events to visualize the
 * MODULE in action in Paraver traces.
 */
#include "support/queues.h"
#include "support/tracing.h"
/*
 * When we use DROM we need to transmit information to the threads that are part
 * of the OpenMP team so them can change the mask. To communicate between
 * different threads of the same process we have used atomic variables.
 */
#include "apis/dlb_types.h"
#include "support/types.h"

#include "support/mask_utils.h"

#include <sched.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * To ceil the drom balancing.
 */
#include <math.h>

#include "mngo/mngo_talp.h"

/*
 * True when mngo has been initialized correctly
 */
static bool have_mngo = false;
static bool have_drom = false;
static bool have_lewi = false;

static bool have_helper_thread = false;
static __thread bool im_helper_thread = false;

static const char *helper_thread_region_name = "helper-thread";

/*
 * To take decisions we need a history of performance to make an educated guess
 * on what is the best action to improve performance.
 */

#include <LB_comm/shmem_cpuinfo.h>

static mngo_actions_t mngo_actions(const subprocess_descriptor_t *spd,
                                   const mngo_reduced_metric_t *metrics) {
    mngo_info_t *mngo_info = spd->mngo_info;

    // Load balance in deviation: is the percentage of parallel efficiency
    // difference of this process with relation to the node average.
    float lb_in_deviation = (metrics->self_pe / metrics->shared_pe) - 1;

    // We consider that there is LB_IN if the deviation absolute value of
    // deviation is greater than deviation.
    bool lb_in = (lb_in_deviation > +mngo_info->lb_in_threshold ||
                  lb_in_deviation < -mngo_info->lb_in_threshold);

    /*
     * For now only one decision can be taken per process each time. So when
     * any policy is activated we return.
     */
    mngo_actions_t action = MNGO_ACTION_NONE;
    if (lb_in) {
        if (have_drom) {
            // TODO: in the event where both LeWI and DROM are enabled we try
            // first with DROM and then if we observe performance degradation/no
            // performance increase we skip this and try with LeWI.
            action = MNGO_ACTION_DROM;
            goto policy_applied;
        }

        if (have_lewi) {
            action = MNGO_ACTION_LEWI_START;
            goto policy_applied;
        }
    }
policy_applied:

    // Share the actions accross nodes to aggree on the new decission
    shmem_mngo__alltoall_actions(mngo_info->mid, &action);

    return action;
}

/*
 * Finally we take the actions that have been collectively decided and we apply
 * them.
 */
static void mngo_apply_actions(subprocess_descriptor_t *spd,
                               mngo_actions_t *actions, cpu_set_t *next_mask) {
    mngo_info_t *mngo_info = spd->mngo_info;
    mngo_state_t *state =
        mngo_regions__get_current_resources(&mngo_info->region_handler);

    switch (*actions) {
    case MNGO_ACTION_LEWI_START: {
        verbose(VB_MNGO, "Starting LeWI");
        instrument_event(MNGO_MANAGER, EVENT_MNGO_LEWI_ON, EVENT_BEGINEND);
        state->lewi_on = true;
        break;
    }
    case MNGO_ACTION_LEWI_STOP: {
        verbose(VB_MNGO, "Stopping LeWI");
        instrument_event(MNGO_MANAGER, EVENT_MNGO_LEWI_OFF, EVENT_BEGINEND);
        state->lewi_on = false;
        break;
    }
    case MNGO_ACTION_DROM: {
        // instrument_event done outside this function, in mngo_manger
        memcpy(&state->drom_mask, next_mask, sizeof(cpu_set_t));
        break;
    }
    default:
        break;
    }
}

/*
 * The MPI collectives mode will run the mngo_manager in the same thread that
 * is executing while an MPI collective call with the MPI_COMM_WORLD
 * communicator is used. Since the most applications usually have some
 * consecutive collective calls. To prevent calling the mngo_manager to
 * continuously we use the mngo interval option as a cool-down time while all
 * collectives will be ignored and no manager will be called.
 *
 * This mode will directly call the mngo_manager function to run the MNGO core
 * functionality.
 */
int mngo_manager(subprocess_descriptor_t *spd, dlb_mngo_region_t *region,
                 mngo_manager_entrypoint_t entrypoint) {

    int error = DLB_SUCCESS;

    mngo_info_t *mngo_info = spd->mngo_info;
    mngo_regions_manager_t *region_manager = &mngo_info->region_handler;

    if (entrypoint & MANAGER_END) {
        if (!mngo_regions__check_top(region_manager, region)) {
            return DLB_NOUPDT;
        }

        mngo_region_info_t *region_info = &region->region_info;

        // Collect metrics and update metrics history
        {
            instrument_event(MNGO_MANAGER, EVENT_MNGO_TALP, EVENT_BEGIN);
            region_stop(spd, region_info->talp.monitor); // TALP region stop
            mngo_talp_metrics_t *metrics =
                mngo_metrics__history_emplace(&region_info->metrics_history);
            mngo_talp__take_metrics(spd, &region_info->talp, metrics);
            instrument_event(MNGO_MANAGER, EVENT_MNGO_TALP, EVENT_END);
        }

        {
            instrument_event(MNGO_MANAGER, EVENT_MNGO_HISTORY, EVENT_BEGIN);
            mngo_reduced_metric_t metrics;
            mngo_metrics__history_tendency(&region_info->metrics_history,
                                           &metrics);
            instrument_event(MNGO_MANAGER, EVENT_MNGO_HISTORY, EVENT_END);

            instrument_event(MNGO_MANAGER, EVENT_MNGO_DECIDE, EVENT_BEGIN);
            mngo_actions_t actions = mngo_actions(spd, &metrics);
            instrument_event(MNGO_MANAGER, EVENT_MNGO_DECIDE, EVENT_END);

            cpu_set_t new_cpu_mask;
            if (MNGO_ACTION_DROM == actions) {
                instrument_event(MNGO_MANAGER, EVENT_MNGO_DROM_IN, EVENT_BEGIN);
                mngo_drom__balance(spd, metrics.self_pe, metrics.shared_pe,
                                   &new_cpu_mask);
            }

            mngo_apply_actions(spd, &actions, &new_cpu_mask);

            if (MNGO_ACTION_DROM == actions) {
                instrument_event(MNGO_MANAGER, EVENT_MNGO_DROM_IN, EVENT_END);
            }
        }

        mngo_regions__pop_active(region_manager, region);
        mngo_state__load(spd,
                         mngo_regions__get_current_resources(region_manager));

        shmem_mngo_barrier_wait();
    }

    if (entrypoint & MANAGER_BEGIN) {
        if (mngo_regions__check_top(region_manager, region)) {
            return DLB_NOUPDT;
        }

        mngo_regions__push_active(region_manager, region);
        mngo_state__load(spd,
                         mngo_regions__get_current_resources(region_manager));

        mngo_region_info_t *region_info = &region->region_info;

        region_start(spd, region_info->talp.monitor); // TALP region start

        shmem_mngo_barrier_wait();
    }

    return error; // TODO: track errors
}

/*
 * The regions mode will implement two calls, one to start the regions and the
 * other to end it. Performance information will then be stored for each
 * region, and each region will have its own profile. The profile will contain
 * the information about which DLB modules are being used in the specific
 * region.
 */

dlb_mngo_region_t *mngo_region_register(const subprocess_descriptor_t *spd,
                                        const char *name) {
    if (!have_mngo) return NULL;
    if (have_helper_thread && !im_helper_thread) return NULL;

    verbose(VB_MNGO, "Registering region %s", name);

    mngo_info_t *mngo_info = spd->mngo_info;
    mngo_regions_manager_t *region_manager = &mngo_info->region_handler;

    dlb_mngo_region_t *region_found = mngo_regions__find(region_manager, name);
    if (NULL != region_found) {
        return region_found;
    }

    /**
     * If the region does not exists we create a new one.
     */
    mngo_state_t *current_state =
        mngo_regions__get_current_resources(region_manager);

    dlb_mngo_region_t *region = mngo_regions__alloc(region_manager);
    if (NULL == region) {
        return NULL;
    }

    // The new region inherits the current state
    region->region_info.state = *current_state;

    // We create a monitoring region for the new region
    int region_register_error =
        mngo_talp__region_register(spd, name, &region->region_info.talp);
    if (DLB_SUCCESS != region_register_error) {
        return NULL;
    }

    // The region name lives in the TALP monitor.
    region->name = mngo_talp__get_region_name(&region->region_info.talp);

    // We initialize the performance history of the region
    mngo_metrics__history_init(&region->region_info.metrics_history);

    return region;
}

int mngo_region_start(subprocess_descriptor_t *spd, dlb_mngo_region_t *region) {
    int error = DLB_SUCCESS;

    if (!have_mngo) return DLB_ERR_NOMNGO;

    if (!have_helper_thread) {
        verbose(VB_MNGO, "Starting region %s", region->name);

        error = mngo_manager(spd, region, MANAGER_BEGIN);
        if (error != DLB_SUCCESS) goto bail_on_error;

        // the if stops here because if we are running with helper-thread we
        // will take the oportunity to call poll_drom_update.
    }

    if (have_drom) {
        error = poll_drom_update(spd);
        // No change is spossible so we translate to success
        if (error == DLB_NOUPDT) error = DLB_SUCCESS;
        if (error != DLB_SUCCESS) goto bail_on_error;
    }

bail_on_error:
    return error;
}

int mngo_region_stop(subprocess_descriptor_t *spd, dlb_mngo_region_t *region) {
    int error = DLB_SUCCESS;

    if (!have_mngo) return DLB_ERR_NOMNGO;

    if (!have_helper_thread) {
        verbose(VB_MNGO, "Stopping region %s", region->name);
        mngo_info_t *mngo_info = spd->mngo_info;
        shmem_barrier__barrier(mngo_info->dlb_node_barrier);

        error = mngo_manager(spd, region, MANAGER_END);
        if (error != DLB_SUCCESS) goto bail_on_error;

        // the if stops here because if we are running with helper-thread we
        // will take the oportunity to call poll_drom_update.
    }

    if (have_drom) {
        error = poll_drom_update(spd);
        // No change is spossible so we translate to success
        if (error == DLB_NOUPDT) error = DLB_SUCCESS;
        if (error != DLB_SUCCESS) goto bail_on_error;
    }

bail_on_error:
    return error;
}

/*
 * The helper thread mode will spawn a pthread that will sleep for an interval
 * of time defined in the options. After the interval of time the thread will
 * wakeup, execute the mngo_manger, which will apply selected MNGO policy.
 */
static void mngo_helper_thread(subprocess_descriptor_t *spd) {

    im_helper_thread = true;
    thread_spd = spd;

    thread_ctx_set_observer(true);

    mngo_info_t *mngo_info = spd->mngo_info;

    verbose(VB_MNGO, "Rank %d started helper thread", mngo_info->rank);

    dlb_mngo_region_t *region =
        mngo_region_register(spd, helper_thread_region_name);

    while (1) {
        /**
         * Only the node representative manager gathers the metrics, takes
         * decisions and sends them to the other managers.
         */
        if (mngo_info->mid == 0) {
            /** Time between MNGO managment events.
             */
            struct timeval now;
            gettimeofday(&now, NULL);

            struct timespec abstime = {
                .tv_sec = now.tv_sec + thread_spd->options.mngo_interval_time,
                .tv_nsec = 0,
            };

            shmem_mngo_manager_wait(&abstime);
        }

        shmem_mngo_barrier_wait();

        if (shmem_mngo_check_stop_flag()) {
            break;
        }

        verbose(VB_MNGO, "Rank %d helper thread has woken up", mngo_info->rank);

        mngo_manager(spd, region, (mngo_manager_entrypoint_t)(MANAGER_BEGIN | MANAGER_END));
    }

    verbose(VB_MNGO, "Rank %d helper thread stopping", mngo_info->rank);

    /**
     * Wait all the managers from the node to finalize.
     */
    shmem_mngo_fini_sync();

    /**
     * Close the shared memory used by the manager thread.
     */
    shmem_mngo_fini(mngo_info->mid);

    // Finalize thread
    pthread_exit(0);
}

/*
 * For the MODULE to work properly some initialization steps have to be taken.
 * We provide the mngo_init function to prepare the MODULE for use. This
 * function will first allocate and initialize the mngo_info, initialize the
 * shared memory, and provide a unique (locally in the node) identifier to each
 * caller, create a TALP region for the MODULE, and also a private MPI
 * communicator for the MODULE.
 *
 * This module will finally spawn a helper thread if the `mngo-mode` option is
 * set to `helper-thread`.
 */
static void mngo_helper_thread(subprocess_descriptor_t *spd);
/*
 * For now the `helper-thread` option is the only one available, and the helper
 * thread is created always. In the future other modes will implemented such a
 * `mpi-collectives` mode which will call the mngo manager on MPI collective
 * calls, or the `region` mode, where using an API the developer will define
 * different regions in the execution, and then MNGO will create automatic
 * profiles for the different regions.
 */
void mngo_init(subprocess_descriptor_t *spd) {
    if (have_mngo) return;

    if (spd->options.lewi) {
        have_lewi = true;
    }

    if (spd->options.drom) {
        have_drom = true;
    }

    if (spd->options.mngo_mode == MNGO_HELPER_THREAD) {
        have_helper_thread = true;
#ifdef MPI_LIB
        int any_abort_init, abort_init = 0;
        int mpi_thread_provided;
        int error = PMPI_Query_thread(&mpi_thread_provided);

        if (error != MPI_SUCCESS) {
            warning0("Could not check the MPI thread level, required for "
                     "helper-thread mode.");
            abort_init = 1;
        }

        if (mpi_thread_provided < MPI_THREAD_MULTIPLE) {
            abort_init = 1;
        }

        PMPI_Allreduce(&abort_init, &any_abort_init, 1, MPI_INT, MPI_SUM,
                       MPI_COMM_WORLD);
        if (any_abort_init > 0) {
            warning0("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
                     "!!!!!!!!!");
            warning0("!!! Trying to initialize MNGO with helper-thread mode, "
                     "but the !!!");
            warning0("!!! MPI thread level is lower than MPI_THREAD_MULTIPLE.  "
                     "      !!!");
            warning0("!!! ABORTING MNGO INITIALIZATION                         "
                     "      !!!");
            warning0("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
                     "!!!!!!!!!");
            spd->mngo_info = NULL;
            return;
        }
#endif // MPI_LIB
    }

    mngo_info_t *mngo_info = (mngo_info_t *)calloc(1, sizeof(mngo_info_t));

#ifdef MPI_LIB
    int rank;
    PMPI_Comm_rank(MPI_COMM_WORLD, &rank);
    PMPI_Comm_dup(MPI_COMM_WORLD, &mngo_info->comm);
#else
    int rank = 0;
#endif

    /**
     * Initialize this process manegers shared memory.
     */
    pid_t pid = spd->id;
    if (shmem_mngo_init(spd->options.shm_key, pid, rank, &mngo_info->mid) !=
        DLB_SUCCESS) {
        // DO SOMETHING
        fatal("MNGO Shared memory initialization failed.");
    }

    mngo_info->rank = rank;

    cpu_set_t mask;
    shmem_procinfo__getprocessmask(shmem_mngo_get_pid(mngo_info->mid), &mask,
                                   DLB_DROM_FLAGS_NONE);

    mngo_info->default_state = (const mngo_state_t){
        .lewi_on = false,
    };
    memcpy(&mngo_info->default_state.drom_mask, &mask, sizeof(cpu_set_t));

    mngo_regions__manager_init(&mngo_info->region_handler,
                               &mngo_info->default_state);

    /**
     * Configure MNGO based on options.
     */
    mngo_info->lb_in_threshold = ((float) spd->options.mngo_lb_in_threshold) / 100.0;
    mngo_info->lb_out_threshold = ((float) spd->options.mngo_lb_out_threshold) / 100.0;

    /**
     * Create a barrier_t to limit regions
     */
    mngo_info->dlb_node_barrier =
        node_barrier_register(spd, "__mngo", DLB_BARRIER_LEWI_ON);
    shmem_mngo_barrier_wait(); // Ensure that all processes of the node attached

    /**
     * Store the MNGO info to the SubProcessDescriptor.
     */
    spd->mngo_info = mngo_info;

    /**
     * Initialize the MNGO manager thread for this process.
     */
    if (spd->options.mngo_mode == MNGO_HELPER_THREAD) {
        pthread_create(&mngo_info->thread, NULL,
                       (void *(*)(void *))mngo_helper_thread, (void *)spd);
    }

    // MNGO initialized successfully
    have_mngo = 1;
    verbose(VB_MNGO, "MNGO Initialized successfully.");
}

void mngo_fini(const subprocess_descriptor_t *spd) {

    if (!have_mngo) return;
    have_mngo = false;

    mngo_info_t *mngo_info = spd->mngo_info;

    // If MNGO was never initialized just return.
    if (mngo_info == NULL) {
        return;
    }

    if (true /* TODO mngo_info->shmem_active */) {

#ifdef MPI_LIB
        PMPI_Barrier(MPI_COMM_WORLD);
#endif

        /**
         * Send the stop message throug the shm and signal the manager thread to
         * wake up.
         */
        if (mngo_info->mid == 0) {
            shmem_mngo_manager_wake_finalize();
        }
    }

    /**
     * Join with the manager thread.
     */
    if (spd->options.mngo_mode == MNGO_HELPER_THREAD) {
        int *retval;
        pthread_join(mngo_info->thread, (void **)&retval);
    }

    /**
     * Free the TALP monitoring regions.
     */
    queue_iter_head2tail_t iter =
        queue__into_head2tail_iter(mngo_info->region_handler.regions);
    dlb_mngo_region_t **current = NULL;
    while ((current = queue_iter__get_nth(&iter, 0)) != NULL) {
        region_stop(spd,
                    (*current)->region_info.talp.monitor); // TALP region stop
        mngo_metrics__history_fini(&(*current)->region_info.metrics_history);
    }

    mngo_regions__manager_finalize(&mngo_info->region_handler);

    if (!have_helper_thread) {
        // The shared memory is managed by the helper-thread if available

        /**
         * Wait all the managers from the node to finalize.
         */
        shmem_mngo_fini_sync();

        /**
         * Close the shared memory used by the manager thread.
         */
        shmem_mngo_fini(mngo_info->mid);
    }

    /**
     * Free the mngo_info_t struct from the sub-process descriptor.
     */
    free(mngo_info);
    mngo_info = NULL;
}

int mngo_manager_wake(const subprocess_descriptor_t *spd) {

    mngo_info_t *mngo_info = spd->mngo_info;

    if (mngo_info->mid == 0) {
        shmem_mngo_manager_wake();
    }

    return DLB_SUCCESS;
}
