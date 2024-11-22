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

#include "apis/dlb.h"
#include "apis/dlb_talp.h"
#include "support/env.h"
#include "support/mask_utils.h"
#include "LB_comm/shmem.h"
#include "LB_core/DLB_talp.h"
#include "LB_core/spd.h"

#include <float.h>
#include <limits.h>
#include <math.h>
#include <pthread.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>


extern __thread bool thread_is_observer;

void __gcov_flush() __attribute__((weak));

struct data {
    pthread_barrier_t barrier;
};

static shmem_handler_t* open_shmem(void **shdata) {
    return shmem_init(shdata,
            &(const shmem_props_t) {
                .size = sizeof(struct data),
                .name = "test",
                .key = SHMEM_KEY,
            });
}

int main(int argc, char *argv[]) {
    char dlb_args[64] = "--talp --talp-external-profiler --shm-key=";
    strcat(dlb_args, SHMEM_KEY);
    dlb_setenv("DLB_ARGS", dlb_args, NULL, ENV_APPEND);

    /* Test DLB_TALP_Attach without any process */
    {
        int ncpus, nelems, pidlist;
        double mpi_time, useful_time;
        assert( DLB_TALP_Attach() == DLB_SUCCESS );
        assert( DLB_TALP_GetNumCPUs(&ncpus) == DLB_SUCCESS );
        assert( ncpus == mu_get_system_size() );
        assert( DLB_TALP_GetPidList(&pidlist, &nelems, 1) == DLB_SUCCESS );
        assert( nelems == 0 );
        assert( DLB_TALP_GetTimes(111, &mpi_time, &useful_time) == DLB_ERR_NOPROC );
        assert( DLB_TALP_Detach() == DLB_SUCCESS );
    }

    /* Test that an external process should not call these functions */
    {
        assert( DLB_TALP_Attach() == DLB_SUCCESS );
        assert( DLB_MonitoringRegionGetGlobal() == NULL );
        assert( DLB_MonitoringRegionRegister("Name") == NULL );
        assert( DLB_MonitoringRegionReset(DLB_GLOBAL_REGION) == DLB_ERR_NOTALP );
        assert( DLB_MonitoringRegionStart(DLB_GLOBAL_REGION) == DLB_ERR_NOTALP );
        assert( DLB_MonitoringRegionStop(DLB_GLOBAL_REGION) == DLB_ERR_NOTALP );
        assert( DLB_MonitoringRegionReport(DLB_GLOBAL_REGION) == DLB_ERR_NOTALP );
        assert( DLB_MonitoringRegionsUpdate() == DLB_ERR_NOTALP );
        assert( DLB_TALP_CollectPOPMetrics(NULL, NULL) == DLB_ERR_NOTALP );
        assert( DLB_TALP_CollectPOPNodeMetrics(NULL, NULL) == DLB_ERR_NOTALP );
        assert( DLB_TALP_Detach() == DLB_SUCCESS );
    }

    /* Test DLB_TALP_Attach after DLB_Init */
    {
        // Check thread is NOT an observer and GetTimes are correct
        double mpi_time, useful_time;
        assert( DLB_Init(0, 0, NULL) == DLB_SUCCESS );
        assert( DLB_MonitoringRegionStart(DLB_MPI_REGION) == DLB_SUCCESS );
        assert( DLB_MonitoringRegionStop(DLB_MPI_REGION) == DLB_SUCCESS );
        assert( DLB_TALP_Attach() == DLB_SUCCESS );
        assert( !thread_is_observer );
        assert( DLB_TALP_GetTimes(0, &mpi_time, &useful_time) == DLB_SUCCESS );
        assert( mpi_time < DBL_EPSILON );
        assert( useful_time > DBL_EPSILON );
        assert( DLB_TALP_Detach() == DLB_SUCCESS );
        assert( DLB_Finalize() == DLB_SUCCESS );

        // Save DLB_ARGS, set new one without --talp
        char *dlb_args_env = getenv("DLB_ARGS");
        size_t dlb_args_len = strlen(dlb_args_env) + 1;
        char *dlb_args_copy = malloc(sizeof(char) * dlb_args_len);
        memcpy(dlb_args_copy, dlb_args_env, dlb_args_len);
        char new_dlb_args[32] = "--shm-key=";
        strcat(new_dlb_args, SHMEM_KEY);
        dlb_setenv("DLB_ARGS", new_dlb_args, NULL, ENV_OVERWRITE_ALWAYS);

        // Check functions that cannot be called from 1st-party if not --talp
        assert( DLB_Init(0, 0, NULL) == DLB_SUCCESS );
        assert( DLB_TALP_Attach() == DLB_SUCCESS );
        assert( DLB_TALP_GetTimes(0, &mpi_time, &useful_time) == DLB_ERR_NOTALP );
        assert( DLB_TALP_Detach() == DLB_SUCCESS );
        assert( DLB_Finalize() == DLB_SUCCESS );

        // Set DLB_ARGS with just --talp
        dlb_setenv("DLB_ARGS", " --talp", NULL, ENV_APPEND);

        // Check functions that cannot be called from 1st-party if not --talp-external-profiler
        dlb_node_times_t node_times;
        dlb_node_metrics_t node_metrics;
        assert( DLB_Init(0, 0, NULL) == DLB_SUCCESS );
        assert( DLB_TALP_Attach() == DLB_SUCCESS );
        assert( DLB_TALP_GetNodeTimes(DLB_MPI_REGION, &node_times, NULL, 1) == DLB_ERR_NOSHMEM );
        assert( DLB_TALP_QueryPOPNodeMetrics(DLB_MPI_REGION, &node_metrics) == DLB_ERR_NOSHMEM );
        assert( DLB_TALP_Detach() == DLB_SUCCESS );
        assert( DLB_Finalize() == DLB_SUCCESS );

        // Restore DLB_ARGS
        dlb_setenv("DLB_ARGS", dlb_args_copy, NULL, ENV_OVERWRITE_ALWAYS);
        free(dlb_args_copy);

        // Check thread may be an oberver again
        assert( DLB_TALP_Attach() == DLB_SUCCESS );
        assert( thread_is_observer );
        assert( DLB_TALP_Detach() == DLB_SUCCESS );
    }

    /* Test DLB_TALP_Attach before other processes */
    {
        struct data *shdata;
        shmem_handler_t *handler;

        // Parent process creates shmem and initializes barrier
        handler = open_shmem((void**)&shdata);
        pthread_barrierattr_t attr;
        assert( pthread_barrierattr_init(&attr) == 0 );
        assert( pthread_barrierattr_setpshared(&attr, PTHREAD_PROCESS_SHARED) == 0 );
        assert( pthread_barrier_init(&shdata->barrier, &attr, 2) == 0 );
        assert( pthread_barrierattr_destroy(&attr) == 0 );

        // Parent does TALP_Attach
        int nelems, pidlist;
        assert( DLB_TALP_Attach() == DLB_SUCCESS );
        assert( DLB_TALP_GetPidList(&pidlist, &nelems, 1) == DLB_SUCCESS );
        assert( nelems == 0 );

        // Fork
        pid_t pid = fork();
        assert( pid >= 0 );
        bool child = pid == 0;
        bool parent = !child;

        if (child) {
            assert( DLB_Init(0, 0, NULL) == DLB_SUCCESS );
            talp_mpi_init(thread_spd);
            handler = open_shmem((void**)&shdata);
        }

        int error = pthread_barrier_wait(&shdata->barrier);
        assert(error == 0 || error == PTHREAD_BARRIER_SERIAL_THREAD);

        // Parent process checks existing process with empty MPI and useful values
        if (parent) {
            assert( DLB_TALP_GetPidList(&pidlist, &nelems, 1) == DLB_SUCCESS );
            assert( nelems == 1 && pidlist == pid);
            double mpi_time, useful_time;
            assert( DLB_TALP_GetTimes(pid, &mpi_time, &useful_time) == DLB_SUCCESS );
            assert( fabs(mpi_time) < 1e-6 && fabs(useful_time) < 1e-6 );
        }

        error = pthread_barrier_wait(&shdata->barrier);
        assert(error == 0 || error == PTHREAD_BARRIER_SERIAL_THREAD);

        // Child process simulates some time in computation
        if (child) {
            usleep(1000); /* 1 ms */
            assert( DLB_MonitoringRegionsUpdate() == DLB_SUCCESS );
        }

        error = pthread_barrier_wait(&shdata->barrier);
        assert(error == 0 || error == PTHREAD_BARRIER_SERIAL_THREAD);

        // Parent process checks times again
        if (parent) {
            double mpi_time, useful_time;
            assert( DLB_TALP_GetTimes(pid, &mpi_time, &useful_time) == DLB_SUCCESS );
            assert( fabs(mpi_time) < 1e-6 );
            assert( fabs(useful_time) > 1e-3 && fabs(useful_time) < 1 );
        }

        error = pthread_barrier_wait(&shdata->barrier);
        assert(error == 0 || error == PTHREAD_BARRIER_SERIAL_THREAD);

        if (child) {
            assert( DLB_Finalize() == DLB_SUCCESS );
            shmem_finalize(handler, NULL);

            // We need to call _exit so that childs don't call assert_shmem destructors,
            // but that prevents gcov reports, so we'll call it if defined
            if (__gcov_flush) __gcov_flush();
            _exit(EXIT_SUCCESS);
        }
        /* if (child) _exit(EXIT_SUCCESS); */

        // Wait for all child processes
        int wstatus;
        while(wait(&wstatus) > 0) {
            if (!WIFEXITED(wstatus))
                exit(EXIT_FAILURE);
            int rc = WEXITSTATUS(wstatus);
            if (rc != 0) {
                printf("Child return status: %d\n", rc);
                exit(EXIT_FAILURE);
            }
        }

        // Parent process finalizes everything
        assert( pthread_barrier_destroy(&shdata->barrier) == 0 );
        shmem_finalize(handler, NULL);
        assert( DLB_TALP_Detach() == DLB_SUCCESS );
    }

    /* Test DLB_TALP_Attach after other process has initialized */
    {
        struct data *shdata;
        shmem_handler_t *handler;

        // Parent process creates shmem and initializes barrier
        handler = open_shmem((void**)&shdata);
        pthread_barrierattr_t attr;
        assert( pthread_barrierattr_init(&attr) == 0 );
        assert( pthread_barrierattr_setpshared(&attr, PTHREAD_PROCESS_SHARED) == 0 );
        assert( pthread_barrier_init(&shdata->barrier, &attr, 2) == 0 );
        assert( pthread_barrierattr_destroy(&attr) == 0 );

        // Fork
        pid_t pid = fork();
        assert( pid >= 0 );
        bool child = pid == 0;
        bool parent = !child;

        if (child) {
            assert( DLB_Init(0, 0, NULL) == DLB_SUCCESS );
            handler = open_shmem((void**)&shdata);
        }

        int error = pthread_barrier_wait(&shdata->barrier);
        assert(error == 0 || error == PTHREAD_BARRIER_SERIAL_THREAD);

        if (parent) {
            // Parent does TALP_Attach
            int nelems, pidlist;
            assert( DLB_TALP_Attach() == DLB_SUCCESS );
            assert( DLB_TALP_GetPidList(&pidlist, &nelems, 1) == DLB_SUCCESS );
            assert( nelems == 1 && pidlist == pid);
            assert( DLB_TALP_Detach() == DLB_SUCCESS );
        }

        error = pthread_barrier_wait(&shdata->barrier);
        assert(error == 0 || error == PTHREAD_BARRIER_SERIAL_THREAD);

        if (child) {
            assert( DLB_Finalize() == DLB_SUCCESS );
            shmem_finalize(handler, NULL);

            // We need to call _exit so that childs don't call assert_shmem destructors,
            // but that prevents gcov reports, so we'll call it if defined
            if (__gcov_flush) __gcov_flush();
            _exit(EXIT_SUCCESS);
        }
        /* if (child) _exit(EXIT_SUCCESS); */

        // Wait for all child processes
        int wstatus;
        while(wait(&wstatus) > 0) {
            if (!WIFEXITED(wstatus))
                exit(EXIT_FAILURE);
            int rc = WEXITSTATUS(wstatus);
            if (rc != 0) {
                printf("Child return status: %d\n", rc);
                exit(EXIT_FAILURE);
            }
        }

        // Parent process finalizes everything
        assert( pthread_barrier_destroy(&shdata->barrier) == 0 );
        shmem_finalize(handler, NULL);
    }

    /* Test DLB_TALP_GetNodeTimes and DLB_TALP_QueryPOPNodeMetrics */
    {
        struct data *shdata;
        shmem_handler_t *handler;

        // Parent process creates shmem and initializes barrier
        handler = open_shmem((void**)&shdata);
        pthread_barrierattr_t attr;
        assert( pthread_barrierattr_init(&attr) == 0 );
        assert( pthread_barrierattr_setpshared(&attr, PTHREAD_PROCESS_SHARED) == 0 );
        assert( pthread_barrier_init(&shdata->barrier, &attr, 3) == 0 );
        assert( pthread_barrierattr_destroy(&attr) == 0 );

        // Parent does TALP_Attach
        int nelems, pidlist;
        assert( DLB_TALP_Attach() == DLB_SUCCESS );
        assert( DLB_TALP_GetPidList(&pidlist, &nelems, 1) == DLB_SUCCESS );
        assert( nelems == 0 );

        // Fork twice
        pid_t pid2 = -1;
        pid_t pid1 = fork();
        assert( pid1 >= 0 );
        if (pid1 > 0) {
            pid2 = fork();
            assert( pid2 >= 0 );
        }
        bool child = pid1 == 0 || pid2 == 0;
        bool parent = !child;

        if (child) {
            assert( DLB_Init(0, 0, NULL) == DLB_SUCCESS );
            talp_mpi_init(thread_spd);
            handler = open_shmem((void**)&shdata);
        }

        int error = pthread_barrier_wait(&shdata->barrier);
        assert(error == 0 || error == PTHREAD_BARRIER_SERIAL_THREAD);

        // Children simulate some time in computation
        if (child) {
            usleep(1000); /* 1 ms */
            assert( DLB_MonitoringRegionsUpdate() == DLB_SUCCESS );
        }

        error = pthread_barrier_wait(&shdata->barrier);
        assert(error == 0 || error == PTHREAD_BARRIER_SERIAL_THREAD);

        // Parent process test API
        if (parent) {
            // Test DLB_TALP_GetNodeTimes
            dlb_node_times_t node_times[2];
            assert( DLB_TALP_GetNodeTimes(DLB_GLOBAL_REGION, node_times, &nelems, 1)
                    == DLB_SUCCESS );
            assert( nelems == 1);
            assert( node_times[0].pid == pid1 || node_times[0].pid == pid2 );

            assert( DLB_TALP_GetNodeTimes(DLB_GLOBAL_REGION, node_times, &nelems, 2)
                    == DLB_SUCCESS );
            assert( nelems == 2);

            assert( DLB_TALP_GetNodeTimes(DLB_GLOBAL_REGION, node_times, &nelems, INT_MAX )
                    == DLB_SUCCESS );
            assert( nelems == 2);
            fprintf(stderr, "0 pid: %d\n", node_times[0].pid);
            fprintf(stderr, "1 pid: %d\n", node_times[1].pid);
            fprintf(stderr, "pid1: %d, pid2: %d\n", pid1, pid2);
            assert( node_times[0].pid == min_int(pid1, pid2) );
            assert( node_times[1].pid == max_int(pid1, pid2) );
            assert( node_times[0].mpi_time == 0 && node_times[1].mpi_time == 0 );
            assert( llabs(node_times[0].useful_time) > 1e6
                    && llabs(node_times[0].useful_time) < 1e9 );
            assert( llabs(node_times[1].useful_time) > 1e6
                    && llabs(node_times[1].useful_time) < 1e9 );

            // Test DLB_TALP_QueryPOPNodeMetrics
            dlb_node_metrics_t node_metrics;
            assert( DLB_TALP_QueryPOPNodeMetrics(DLB_GLOBAL_REGION, &node_metrics)
                    == DLB_SUCCESS );
            assert( node_metrics.processes_per_node == 2 );
            assert( node_metrics.total_mpi_time == 0 );
            assert( node_metrics.total_useful_time > 1e6 && node_metrics.total_useful_time < 2e9 );
            assert( fabs(node_metrics.communication_efficiency - 1.00) < 1e3 );
        }

        error = pthread_barrier_wait(&shdata->barrier);
        assert(error == 0 || error == PTHREAD_BARRIER_SERIAL_THREAD);

        if (child) {
            assert( DLB_Finalize() == DLB_SUCCESS );
            shmem_finalize(handler, NULL);

            // We need to call _exit so that childs don't call assert_shmem destructors,
            // but that prevents gcov reports, so we'll call it if defined
            if (__gcov_flush) __gcov_flush();
            _exit(EXIT_SUCCESS);
        }
        /* if (child) _exit(EXIT_SUCCESS); */

        // Wait for all child processes
        int wstatus;
        while(wait(&wstatus) > 0) {
            if (!WIFEXITED(wstatus))
                exit(EXIT_FAILURE);
            int rc = WEXITSTATUS(wstatus);
            if (rc != 0) {
                printf("Child return status: %d\n", rc);
                exit(EXIT_FAILURE);
            }
        }

        // Parent process finalizes everything
        assert( pthread_barrier_destroy(&shdata->barrier) == 0 );
        shmem_finalize(handler, NULL);
        assert( DLB_TALP_Detach() == DLB_SUCCESS );
    }

    return 0;
}
