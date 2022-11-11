/*********************************************************************************/
/*  Copyright 2009-2021 Barcelona Supercomputing Center                          */
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
#include "LB_core/spd.h"
#include "LB_comm/shmem_procinfo.h"
#include "apis/dlb_talp.h"
#include "apis/dlb_errors.h"
#include "support/atomic.h"
#include "support/mask_utils.h"
#include "support/mytime.h"

#include <float.h>
#include <sched.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Test TALP node metrics */


typedef struct DLB_ALIGN_CACHE talp_sample_t {
    atomic_int_least64_t    mpi_time;
    atomic_int_least64_t    useful_time;
    atomic_uint_least64_t   num_mpi_calls;
    int64_t     last_updated_timestamp;
    bool        in_useful;
    bool        cpu_disabled;
} talp_sample_t;

typedef struct talp_info_t {
    dlb_monitor_t       mpi_monitor;
    bool                external_profiler;
    int                 ncpus;
    talp_sample_t       **samples;
    pthread_mutex_t     samples_mutex;
} talp_info_t;


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


int main(int argc, char *argv[]) {
    // This test needs at least room for 2 CPUs
    enum { SYS_SIZE = 2 };
    mu_init();
    mu_testing_set_sys_size(SYS_SIZE);

    char options[64] = "--talp --shm-key=";
    strcat(options, SHMEM_KEY);

    /* Single thread */
    {
        printf("pid: %d\n", getpid());
        /* Init spd */
        subprocess_descriptor_t spd = {.id = 111};
        options_init(&spd.options, options);
        mu_parse_mask("0", &spd.process_mask);
        assert( shmem_procinfo__init(spd.id, /* preinit_pid */ 0, &spd.process_mask,
                NULL, spd.options.shm_key) == DLB_SUCCESS );
        talp_init(&spd);
        talp_info_t *talp_info = spd.talp_info;
        dlb_monitor_t *mpi_monitor = &talp_info->mpi_monitor;

        /* Start MPI monitoring region */
        talp_mpi_init(&spd);

        /* MPI call */
        talp_in_mpi(&spd);
        talp_out_mpi(&spd);

        /* Stop MPI monitoring region so that we can collect its metrics twice
         * with the same values */
        assert( monitoring_region_stop(&spd, mpi_monitor) == DLB_SUCCESS );

        /* Get MPI metrics */
        dlb_node_metrics_t node_metrics1;
        dlb_node_metrics_t node_metrics2;
        assert( talp_collect_node_metrics(&spd, DLB_MPI_REGION, &node_metrics1) == DLB_SUCCESS );
        assert( talp_collect_node_metrics(&spd, mpi_monitor, &node_metrics2) == DLB_SUCCESS );
        assert( !monitoring_region_is_started(&spd, mpi_monitor) );
        assert( cmp_node_metrics(&node_metrics1, &node_metrics2) == 0 );
        print_node_metrics(&node_metrics1);

        /* User defined region */
        dlb_node_metrics_t node_metrics3;
        dlb_monitor_t *monitor = monitoring_region_register(NULL);
        assert( monitoring_region_start(&spd, monitor) == DLB_SUCCESS );
        assert( talp_collect_node_metrics(&spd, monitor, &node_metrics3) == DLB_SUCCESS );
        assert( monitoring_region_is_started(&spd, monitor) );

        /* Finalize */
        talp_mpi_finalize(&spd);
        talp_finalize(&spd);
        assert( shmem_procinfo__finalize(spd.id, /* return_stolen */ false, spd.options.shm_key)
                == DLB_SUCCESS );
    }

    return 0;
}
