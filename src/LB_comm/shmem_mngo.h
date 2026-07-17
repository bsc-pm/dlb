/*********************************************************************************/
/*  Copyright 2009-2023 Barcelona Supercomputing Center                          */
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

#ifndef SHMEM_MNGO_H
#define SHMEM_MNGO_H

#include <sched.h>
#include <stddef.h>
#include <stdbool.h>

#include <sys/time.h>

enum {
    DLB_MNGO_SHM_TRY = 0,
    DLB_MNGO_SHM_FORCE = 1
};

typedef enum {
  MNGO_ACTION_NONE = 0,
  MNGO_ACTION_LEWI_START = 1,
  MNGO_ACTION_LEWI_STOP = 2,
  MNGO_ACTION_DROM = 3,
} mngo_actions_t;

/**
 * Function: shmem_mngo_init
 * -------------------------
 * initializes the shared memory for the MNGO module.
 *
 * shmem_key: it is a unique identifier amongst nodes.
 * pid: the id of the process managed by this mngo manager
 * rank: the MPI rank of the process
 * mid: is a pointer to an integer where the unique identifier within the node
 *      will be written.
 *
 * returns: a status of the function, usually DLB_MNGO_SHM_SUCCESS
 *
 */
int shmem_mngo_init(const char *shmem_key, pid_t pid, int rank, size_t *mid);

/**
 * Function: shmem_mngo_fini
 * -------------------------
 * closes the connection to the shared memory for the MNGO module.
 *
 * mid: the manager id to close
 *
 * returns: a status of the function, usually DLB_MNGO_SHM_SUCCESS
 *
 */
int shmem_mngo_fini(size_t mid);

/*
 * Function: shmem_mngo_fini_sync
 * --------------------------------
 * spin lock until the number of attached processes is 0.
 *
 * Note: this syncronization fulfills the barrier of the manager
 */
void shmem_mngo_fini_sync(void);

/**
 * Function: shmem_mngo_barrier_wait
 * ---------------------------------
 * barrier that waits for all the mngo managers
 */
void shmem_mngo_barrier_wait(void);


void shmem_mngo_set_stop_flag(bool flag);

bool shmem_mngo_check_stop_flag(void);

void shmem_mngo_manager_wake(void);

void shmem_mngo_manager_wake_finalize(void);

void shmem_mngo_manager_wait(const struct timespec *abstime);

pid_t shmem_mngo_get_pid(const size_t mid);

size_t shmem_mngo_get_max_size(void);

/**
 * Actions
 */
void shmem_mngo__alltoall_actions(size_t mid, mngo_actions_t *actions);

/**
 * DROM balancing communication
 */
 
int  shmem_mngo_drom__alltoall_deltas_start(size_t mid, int self_cpu_delta);

void shmem_mngo_drom__alltoall_deltas_finish(size_t mid, int *cpu_deltas);

void shmem_mngo_drom__redistribute(size_t mid, int cpu_change, cpu_set_t *cpu_mask);

void shmem_mngo_drom__print_redistribution(int mid, int dcpus, const char* region_name);

/**
 * Function: shmem_mngo__print_info
 * --------------------------------
 * prints the information currently held in the shared memory
 *
 * shmem_key: is the shmem key passed from the options
 */
void shmem_mngo__print_info(const char *shmem_key);

/**
 * Test functions
 */
void test_shmem_mngo__modify_barrier_participants(int participants);

#endif /* SHMEM_MNGO_H */
