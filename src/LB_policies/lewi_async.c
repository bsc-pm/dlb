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


#include "LB_policies/lewi_async.h"

#include "LB_core/spd.h"
#include "apis/dlb_errors.h"
#include "LB_comm/shmem_lewi_async.h"
#include "LB_comm/shmem_async.h"
#include "LB_numThreads/numThreads.h"
#include "support/debug.h"
#include "support/mask_utils.h"
#include "support/options.h"
#include "support/queues.h"
#include "support/small_array.h"
#include "support/types.h"

#include <sched.h>
#include <limits.h>


typedef struct LeWI_async_info {
    unsigned int prev_requested;
} lewi_info_t;

static int node_size;

static void lewi_async_set_num_threads(const pm_interface_t *pm, unsigned int num_threads) {
    int signed_num_threads = (int)num_threads;
    if (signed_num_threads >= 0) {
        verbose(VB_MICROLB, "Using %d cpus", signed_num_threads);
        update_threads(pm, signed_num_threads);
    }
}

/* Helper Lend function, Lend everything except new_ncpus */
static int lewi_async_Lend_keep_cpus(const subprocess_descriptor_t *spd,
        unsigned int new_ncpus) {

    /* Lend CPUs to the shmem, obtain potential requests */
    unsigned int num_requests;
    unsigned int prev_requested;
    SMALL_ARRAY(lewi_request_t, requests, node_size);
    int error = shmem_lewi_async__lend_keep_cpus(spd->id, new_ncpus,
            requests, &num_requests, node_size, &prev_requested);

    if (error == DLB_SUCCESS) {

        /* Save previously requested CPUs to push request again later */
        if (prev_requested > 0 ) {
            lewi_info_t *lewi_info = spd->lewi_info;
            lewi_info->prev_requested = prev_requested;
        }

        /* Set num_threads = new_ncpus */
        lewi_async_set_num_threads(&spd->pm, new_ncpus);

        /* Trigger other processes's helper thread */
        for (unsigned int i=0; i<num_requests; ++i) {
            shmem_async_set_num_cpus(requests[i].pid, requests[i].howmany);
        }
    }

    return error;
}


int lewi_async_Init(subprocess_descriptor_t *spd) {
    verbose(VB_MICROLB, "LeWI Init");

    unsigned int initial_ncpus = spd->lewi_ncpus;
    node_size = mu_get_system_size();

    spd->lewi_info = malloc(sizeof(lewi_info_t));
    lewi_info_t *lewi_info = spd->lewi_info;
    lewi_info->prev_requested = 0;

    lewi_async_set_num_threads(&spd->pm, initial_ncpus);

    info0("Default cpus per process: %d", initial_ncpus);

    // Initialize shared memory
    shmem_lewi_async__init(spd->id, initial_ncpus, spd->options.shm_key,
            spd->options.shm_size_multiplier);

    return DLB_SUCCESS;
}

int lewi_async_Finalize(subprocess_descriptor_t *spd) {

    unsigned int new_ncpus;
    unsigned int num_requests;
    SMALL_ARRAY(lewi_request_t, requests, node_size);
    shmem_lewi_async__finalize(spd->id, &new_ncpus, requests,
            &num_requests, node_size);

    /* Set num_threads = new_ncpus */
    lewi_async_set_num_threads(&spd->pm, new_ncpus);

    /* Trigger other processes's helper thread */
    for (unsigned int i=0; i<num_requests; ++i) {
        shmem_async_set_num_cpus(requests[i].pid, requests[i].howmany);
    }

    free(spd->lewi_info);
    spd->lewi_info = NULL;

    return DLB_SUCCESS;
}

int lewi_async_Enable(const subprocess_descriptor_t *spd) {

    int error = DLB_SUCCESS;

    /* Restore previously requested CPUs */
    lewi_info_t *lewi_info = spd->lewi_info;
    if (lewi_info->prev_requested > 0) {
        error = lewi_async_AcquireCpus(spd, lewi_info->prev_requested);
        if (error == DLB_SUCCESS || error == DLB_NOTED) {
            lewi_info->prev_requested = 0;
        }
    }

    return error;
}

int lewi_async_Disable(const subprocess_descriptor_t *spd) {
    verbose(VB_MICROLB, "Reset LeWI");

    unsigned int new_ncpus;
    unsigned int num_requests;
    unsigned int prev_requested;
    SMALL_ARRAY(lewi_request_t, requests, node_size);
    int error = shmem_lewi_async__reset(spd->id, &new_ncpus, requests,
            &num_requests, node_size, &prev_requested);

    if (error == DLB_SUCCESS) {

        /* Save previously requested CPUs to push request again later */
        if (prev_requested > 0 ) {
            lewi_info_t *lewi_info = spd->lewi_info;
            lewi_info->prev_requested = prev_requested;
        }

        /* Set num_threads = new_ncpus */
        lewi_async_set_num_threads(&spd->pm, new_ncpus);

        /* Trigger other processes's helper thread */
        for (unsigned int i=0; i<num_requests; ++i) {
            shmem_async_set_num_cpus(requests[i].pid, requests[i].howmany);
        }
    }

    return error == DLB_NOUPDT ? DLB_SUCCESS : error;
}

int lewi_async_IntoBlockingCall(const subprocess_descriptor_t *spd) {
    if (spd->options.lewi_keep_cpu_on_blocking_call) {
        /* 1CPU */
        lewi_async_Lend_keep_cpus(spd, 1);
    } else {
        /* BLOCK */
        lewi_async_Lend_keep_cpus(spd, 0);
    }
    return DLB_SUCCESS;
}

int lewi_async_OutOfBlockingCall(const subprocess_descriptor_t *spd) {
    return lewi_async_Reclaim(spd);
}


/* Lend everything except 1 CPU */
int lewi_async_Lend(const subprocess_descriptor_t *spd) {
    return lewi_async_Lend_keep_cpus(spd, 1);
}

/* Lend ncpus */
int lewi_async_LendCpus(const subprocess_descriptor_t *spd, int ncpus) {

    if (ncpus < 0) return DLB_ERR_PERM;
    if (ncpus == 0) return DLB_NOUPDT;

    /* Lend CPUs to the shmem, obtain potential requests */
    unsigned int new_ncpus;
    unsigned int num_requests;
    unsigned int prev_requested;
    SMALL_ARRAY(lewi_request_t, requests, node_size);
    int error = shmem_lewi_async__lend_cpus(spd->id, ncpus, &new_ncpus,
            requests, &num_requests, node_size, &prev_requested);

    if (error == DLB_SUCCESS) {

        /* Save previously requested CPUs to push request again later */
        if (prev_requested > 0 ) {
            lewi_info_t *lewi_info = spd->lewi_info;
            lewi_info->prev_requested = prev_requested;
        }

        /* Set num_threads = new_ncpus */
        lewi_async_set_num_threads(&spd->pm, new_ncpus);

        /* Trigger other processes's helper thread */
        for (unsigned int i=0; i<num_requests; ++i) {
            shmem_async_set_num_cpus(requests[i].pid, requests[i].howmany);
        }
    }

    return error;
}

int lewi_async_Reclaim(const subprocess_descriptor_t *spd) {
    lewi_info_t *lewi_info = spd->lewi_info;
    unsigned int new_ncpus;
    unsigned int num_requests;
    SMALL_ARRAY(lewi_request_t, requests, node_size);
    int error = shmem_lewi_async__reclaim(spd->id, &new_ncpus, requests,
            &num_requests, node_size, lewi_info->prev_requested);
    if (error == DLB_SUCCESS) {

        /* Reset saved request */
        lewi_info->prev_requested = 0;

        /* Set num_threads = new_ncpus */
        lewi_async_set_num_threads(&spd->pm, new_ncpus);
        verbose(VB_MICROLB, "ACQUIRING %d cpus", new_ncpus);

        /* Trigger other processes's helper thread */
        for (unsigned int i=0; i<num_requests; ++i) {
            shmem_async_set_num_cpus(requests[i].pid, requests[i].howmany);
        }
    }
    return error;
}

int lewi_async_AcquireCpus(const subprocess_descriptor_t *spd, int ncpus) {
    int error = DLB_ERR_UNKNOWN;
    unsigned int new_ncpus;
    unsigned int num_requests = 0;
    SMALL_ARRAY(lewi_request_t, requests, node_size);
    if (ncpus == DLB_DELETE_REQUESTS) {
        /* If ncpus is special value, remove previous requests */
        shmem_lewi_async__remove_requests(spd->id);

        /* Remove also local data */
        lewi_info_t *lewi_info = spd->lewi_info;
        lewi_info->prev_requested = 0;

        error = DLB_SUCCESS;
    } else if (ncpus > 0) {

        /* If there are saved requests and the new one is positive, add them */
        lewi_info_t *lewi_info = spd->lewi_info;
        if (lewi_info->prev_requested > 0) {
            ncpus += lewi_info->prev_requested;
            lewi_info->prev_requested = 0;
        }

        /* Otherwise, borrow as usual, and request remaining CPUs */
        error = shmem_lewi_async__acquire_cpus(spd->id, ncpus, &new_ncpus,
                requests, &num_requests, node_size);

        if (error == DLB_SUCCESS || error == DLB_NOTED) {
            verbose(VB_MICROLB, "Using %d cpus", new_ncpus);
            lewi_async_set_num_threads(&spd->pm, new_ncpus);

            /* Trigger other processes's helper thread */
            for (unsigned int i=0; i<num_requests; ++i) {
                shmem_async_set_num_cpus(requests[i].pid, requests[i].howmany);
            }
        }
    }

    return error;
}

int lewi_async_Borrow(const subprocess_descriptor_t *spd) {
    return lewi_async_BorrowCpus(spd, INT_MAX);
}

int lewi_async_BorrowCpus(const subprocess_descriptor_t *spd, int ncpus) {
    unsigned int new_ncpus;
    int error = shmem_lewi_async__borrow_cpus(spd->id, ncpus, &new_ncpus);
    if (error == DLB_SUCCESS) {
        verbose(VB_MICROLB, "Using %d cpus", new_ncpus);
        lewi_async_set_num_threads(&spd->pm, new_ncpus);
    }
    return error;
}
