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

#include "LB_core/spd.h"
#include "LB_numThreads/omptool.h"
#include "apis/dlb_errors.h"
#include "support/options.h"
#include "talp/regions.h"
#include "talp/talp.h"
#include "talp/talp_openmp.h"
#include "talp/talp_types.h"

#include <sched.h>
#include <pthread.h>
#include <string.h>
#include <assert.h>

/* Test OpenMP TALP */

/* enum { USLEEP_TIME = 1000 }; */

typedef struct parallel_func_args_t {
    int index;
    omptool_parallel_data_t *parallel_data;
    subprocess_descriptor_t *spd;
} parallel_func_args_t;

static void* parallel_func(void *arg) {
    parallel_func_args_t *args = arg;
    int index = args->index;
    omptool_parallel_data_t *parallel_data = args->parallel_data;
    subprocess_descriptor_t *spd = args->spd;

    if (index != 0) {
        spd_enter_dlb(spd);
        talp_openmp_thread_begin();
    }

    talp_openmp_into_parallel_function(parallel_data, index);
    talp_openmp_into_parallel_sync(parallel_data);
    talp_openmp_outof_parallel_sync(parallel_data);
    talp_openmp_task_create();
    talp_openmp_task_switch();
    talp_openmp_task_complete();
    talp_openmp_task_switch();
    talp_openmp_into_parallel_implicit_barrier(parallel_data);

    if (index != 0) {
        talp_openmp_thread_end();
    }

    return NULL;
}

int main(int argc, char *argv[]) {

    int cpu = sched_getcpu();
    cpu_set_t process_mask;
    CPU_ZERO(&process_mask);
    CPU_SET(cpu, &process_mask);

    char options[64] = "--talp --talp-papi --shm-key=";
    strcat(options, SHMEM_KEY);
    subprocess_descriptor_t spd = {.id = 111};
    options_init(&spd.options, options);

    memcpy(&spd.process_mask, &process_mask, sizeof(cpu_set_t));
    spd_enter_dlb(&spd);
    talp_init(&spd);

    talp_info_t *talp_info = spd.talp_info;
    dlb_monitor_t *global_monitor = talp_info->monitor;
    assert( !region_is_started(global_monitor) );

    /* OpenMP Init */
    talp_openmp_init(spd.id, &spd.options);
    talp_openmp_thread_begin();
    assert( region_is_started(global_monitor) );

    /* Parallel region of 1 thread */
    {
        /* Create parallel region */
        omptool_parallel_data_t parallel_data = {
            .level = 1,
            .requested_parallelism = 1,
            .actual_parallelism = 1,
        };
        talp_openmp_parallel_begin(&parallel_data);

        /* Create storage for parallel function */
        parallel_func_args_t args = {
            .index = 0,
            .parallel_data = &parallel_data,
            .spd = &spd,
        };

        /* Primary thred executes parallel function */
        parallel_func(&args);

        /* End parallel */
        talp_openmp_parallel_end(&parallel_data);

        assert( talp_flush_samples_to_regions(&spd) == DLB_SUCCESS );
        assert( global_monitor->num_omp_parallels == 1 );
        assert( global_monitor->num_omp_tasks == 1 );
        assert( global_monitor->useful_time > 0 );
        assert( global_monitor->mpi_time == 0 );
        assert( global_monitor->omp_load_imbalance_time == 0 );
        assert( global_monitor->omp_scheduling_time > 0 );
        assert( global_monitor->omp_serialization_time == 0 );
    }

    /* Parallel region of 2 threads */
    {
        dlb_monitor_t *monitor = region_register(&spd, "Parallel 2");
        assert( monitor != NULL );
        assert( region_start(&spd, monitor) == DLB_SUCCESS );

        /* Create parallel region */
        omptool_parallel_data_t parallel_data = {
            .level = 1,
            .requested_parallelism = 2,
            .actual_parallelism = 2,
        };
        talp_openmp_parallel_begin(&parallel_data);

        /* Create storage for parallel function */
        parallel_func_args_t args[2] = {
            {
                .index = 0,
                .parallel_data = &parallel_data,
                .spd = &spd,
            }, {
                .index = 1,
                .parallel_data = &parallel_data,
                .spd = &spd,
            }
        };

        /* Create worker thread */
        pthread_t worker_thread;
        pthread_create(&worker_thread, NULL, parallel_func, &args[1]);

        /* Primary thread executes parallel function */
        parallel_func(&args[0]);

        /* End parallel */
        pthread_join(worker_thread, NULL);
        talp_openmp_parallel_end(&parallel_data);

        assert( region_stop(&spd, monitor) == DLB_SUCCESS );
        assert( talp_flush_samples_to_regions(&spd) == DLB_SUCCESS );
        assert( global_monitor->num_omp_parallels == 2 );
        assert( monitor->num_omp_parallels == 1 );
        assert( monitor->omp_scheduling_time > 0 );
        assert( monitor->omp_serialization_time > 0 );

        /* The first parallel of 1 thread did not have any LB time, so both
         * monitors should have only the values of the seccond parallel */
        assert( monitor->omp_load_imbalance_time == global_monitor->omp_load_imbalance_time );

        /* Since the second thread was created when the global and the user
         * monitor were started, its time is added as OpenMP serialization to
         * both. The time added to the global monitor is greater because it
         * was started before. */
        assert( monitor->omp_serialization_time < global_monitor->omp_serialization_time );
    }

    talp_finalize(&spd);
    talp_openmp_thread_end();
    talp_openmp_finalize();

    return 0;
}
