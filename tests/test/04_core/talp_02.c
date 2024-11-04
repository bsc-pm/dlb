/*********************************************************************************/
/*  Copyright 2009-2024 Barcelona Supercomputing Center                          */
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

/*<testinfo>
    test_generator="gens/basic-generator"
</testinfo>*/

#include "unique_shmem.h"

#include "LB_core/DLB_talp.h"
#include "LB_core/DLB_kernel.h"
#include "LB_core/node_barrier.h"
#include "LB_core/spd.h"
#include "LB_core/talp_types.h"
#include "LB_comm/shmem_procinfo.h"
#include "apis/dlb_talp.h"
#include "apis/dlb_errors.h"
#include "support/atomic.h"
#include "support/mask_utils.h"
#include "support/mytime.h"

#include <float.h>
#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Test TALP node metrics */


static int cmp_node_metrics(const dlb_node_metrics_t *node_metrics1,
        const dlb_node_metrics_t *node_metrics2) {
    if (node_metrics1->node_id == node_metrics2->node_id
            && node_metrics1->processes_per_node == node_metrics2->processes_per_node
            && node_metrics1->total_useful_time == node_metrics2->total_useful_time
            && node_metrics1->total_mpi_time == node_metrics2->total_mpi_time
            && node_metrics1->max_useful_time == node_metrics2->max_useful_time
            && node_metrics1->max_mpi_time == node_metrics2->max_mpi_time
            && node_metrics1->load_balance - node_metrics2->load_balance < FLT_EPSILON
            && node_metrics1->parallel_efficiency - node_metrics2->parallel_efficiency < FLT_EPSILON
       ) {
        return 0;
    }
    return 1;
}

static void print_node_metrics(const dlb_node_metrics_t *node_metrics) {
    printf( "Node id:             %d\n"
            "Processes per node:  %d\n"
            "Total useful time:   %"PRId64"\n"
            "Total MPI time:      %"PRId64"\n"
            "Max useful time:     %"PRId64"\n"
            "Max MPI time:        %"PRId64"\n"
            "Load Balance:        %f\n"
            "Parallel Eff.:       %f\n",
            node_metrics->node_id,
            node_metrics->processes_per_node,
            node_metrics->total_useful_time,
            node_metrics->total_mpi_time,
            node_metrics->max_useful_time,
            node_metrics->max_mpi_time,
            node_metrics->load_balance,
            node_metrics->parallel_efficiency
          );
}

static void* observer_func(void *arg) {
    subprocess_descriptor_t *spd = arg;
    spd_enter_dlb(spd);

    /* Set up observer flag */
    set_observer_role(true);

    /* Get implicit region */
    const dlb_monitor_t *implicit_monitor = monitoring_region_get_implicit_region(spd);
    assert( implicit_monitor != NULL );
    int num_measurements = implicit_monitor->num_measurements;

    /* An observer may call MPI functions without affecting TALP */
    talp_into_sync_call(spd, true);
    talp_out_of_sync_call(spd, true);
    assert( num_measurements == implicit_monitor->num_measurements );

    /* An observer may not start/stop/update regions */
    assert( monitoring_region_start(spd, DLB_IMPLICIT_REGION) == DLB_ERR_PERM );
    assert( monitoring_region_stop(spd, DLB_IMPLICIT_REGION) == DLB_ERR_PERM );
    assert( monitoring_regions_force_update(spd) == DLB_ERR_PERM );

    /* Get MPI metrics */
    dlb_node_metrics_t node_metrics;
    assert( talp_collect_pop_node_metrics(spd, DLB_IMPLICIT_REGION, &node_metrics) == DLB_SUCCESS );
    assert( node_metrics.processes_per_node == 1 );

    return NULL;
}


int main(int argc, char *argv[]) {
    // This test needs at least room for 2 CPUs
    enum { SYS_SIZE = 2 };
    mu_init();
    mu_testing_set_sys_size(SYS_SIZE);

    char options[64] = "--talp --shm-key=";
    strcat(options, SHMEM_KEY);

    /* This spd needs static storage to spport talp atexit dtor */
    static subprocess_descriptor_t spd = {0};

    /* Single thread */
    {
        printf("pid: %d\n", getpid());
        /* Init spd */
        spd = (const subprocess_descriptor_t){.id = 111};
        spd_enter_dlb(&spd);
        options_init(&spd.options, options);
        mu_parse_mask("0", &spd.process_mask);
        assert( shmem_procinfo__init(spd.id, /* preinit_pid */ 0, &spd.process_mask,
                NULL, spd.options.shm_key) == DLB_SUCCESS );
        shmem_barrier__init(spd.options.shm_key);
        node_barrier_init(&spd);
        talp_init(&spd);
        talp_info_t *talp_info = spd.talp_info;
        dlb_monitor_t *implicit_monitor = talp_info->monitor;

        /* Start implicit monitoring region */
        talp_mpi_init(&spd);

        /* MPI call */
        talp_into_sync_call(&spd, /* is_blocking_collective */ false);
        talp_out_of_sync_call(&spd, /* is_blocking_collective */ false);

        /* Stop implicit monitoring region so that we can collect its metrics twice
         * with the same values */
        assert( monitoring_region_stop(&spd, implicit_monitor) == DLB_SUCCESS );

        /* Get MPI metrics */
        dlb_node_metrics_t node_metrics1;
        dlb_node_metrics_t node_metrics2;
        assert( talp_collect_pop_node_metrics(&spd, DLB_IMPLICIT_REGION, &node_metrics1)
                == DLB_SUCCESS );
        assert( talp_collect_pop_node_metrics(&spd, implicit_monitor, &node_metrics2)
                == DLB_SUCCESS );
        assert( !monitoring_region_is_started(implicit_monitor) );
        assert( cmp_node_metrics(&node_metrics1, &node_metrics2) == 0 );
        print_node_metrics(&node_metrics1);

        /* User defined region */
        dlb_node_metrics_t node_metrics3;
        dlb_monitor_t *monitor = monitoring_region_register(&spd, NULL);
        assert( monitoring_region_start(&spd, monitor) == DLB_SUCCESS );
        assert( talp_collect_pop_node_metrics(&spd, monitor, &node_metrics3) == DLB_SUCCESS );
        assert( monitoring_region_is_started(monitor) );

        /* Finalize */
        talp_mpi_finalize(&spd);
        talp_finalize(&spd);
        node_barrier_finalize(&spd);
        shmem_barrier__finalize(spd.options.shm_key);
        assert( shmem_procinfo__finalize(spd.id, /* return_stolen */ false, spd.options.shm_key)
                == DLB_SUCCESS );
    }

    /* Single thread + observer thread */
    {
        spd = (const subprocess_descriptor_t){.id = 111};
        spd_enter_dlb(&spd);
        options_init(&spd.options, options);
        mu_parse_mask("0", &spd.process_mask);
        assert( shmem_procinfo__init(spd.id, /* preinit_pid */ 0, &spd.process_mask,
                NULL, spd.options.shm_key) == DLB_SUCCESS );
        shmem_barrier__init(spd.options.shm_key);
        node_barrier_init(&spd);
        talp_init(&spd);
        talp_info_t *talp_info = spd.talp_info;
        talp_info->flags.external_profiler = true;

        /* Start implicit monitoring region */
        talp_mpi_init(&spd);

        /* MPI call */
        talp_into_sync_call(&spd, /* is_blocking_collective */ false);
        talp_out_of_sync_call(&spd, /* is_blocking_collective */ false);

        /* Observer threads cannot force-update regions, do it before */
        assert( monitoring_regions_force_update(&spd) == DLB_SUCCESS );

        /* Observer thread is created here */
        pthread_t observer_pthread;
        pthread_create(&observer_pthread, NULL, observer_func, &spd);
        pthread_join(observer_pthread, NULL);

        /* MPI call (force update) */
        talp_into_sync_call(&spd, /* is_blocking_collective */ true);
        talp_out_of_sync_call(&spd, /* is_blocking_collective */ true);

        /* Upon gathering samples, only one thread sample has been reduced */
        assert( talp_info->ncpus == 1 );

        /* Finalize */
        talp_mpi_finalize(&spd);
        talp_finalize(&spd);
        node_barrier_finalize(&spd);
        shmem_barrier__finalize(spd.options.shm_key);
        assert( shmem_procinfo__finalize(spd.id, /* return_stolen */ false, spd.options.shm_key)
                == DLB_SUCCESS );
    }

    return 0;
}
