/*********************************************************************************/
/*  Copyright 2017 Barcelona Supercomputing Center                               */
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
/*  along with DLB.  If not, see <http://www.gnu.org/licenses/>.                 */
/*********************************************************************************/

#include "LB_comm/shmem_async.h"

#include "LB_comm/shmem.h"
#include "LB_comm/shmem_cpuinfo.h"
#include "LB_comm/shmem_procinfo.h"
#include "LB_numThreads/numThreads.h"
#include "apis/dlb_errors.h"
#include "support/mask_utils.h"
#include "support/debug.h"

#include <sched.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <semaphore.h>

#define NOBODY 0

typedef enum HelperAction {
    ACTION_NONE,
    ACTION_ENABLE_CPU,
    ACTION_DISABLE_CPU,
    ACTION_SET_MASK,
    ACTION_JOIN
} action_t;

typedef struct {
    pid_t pid;
    action_t action;
    int cpuid;
    sem_t sem;
    cpu_set_t mask;
    pthread_t thread;
    const pm_interface_t *pm;
} helper_t;

typedef struct {
    helper_t helpers[0];
} shdata_t;

static int max_helpers = 0;
static shdata_t *shdata = NULL;
static const char *shmem_name = "async";
static shmem_handler_t *shm_handler = NULL;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static int subprocesses_attached = 0;

static helper_t* get_helper(pid_t pid) {
    int h;
    for (h = 0; h < max_helpers; ++h) {
        if (shdata->helpers[h].pid == pid) {
            return &shdata->helpers[h];
        }
    }
    return NULL;
}

static void* thread_start(void *arg) {
    helper_t *helper = arg;
    const pm_interface_t const *pm = helper->pm;
    bool join = false;
    while(!join) {
        action_t action;
        int cpuid;

        sem_wait(&helper->sem);
        shmem_lock(shm_handler);
        {
            // We can't attend the petition with the shmem locked
            // We could even consider using atomic exchange operations instead of locks
            action = helper->action;
            cpuid = helper->cpuid;
            helper->action = ACTION_NONE;
        }
        shmem_unlock(shm_handler);

        int error = 0;
        switch(action) {
            case ACTION_NONE:
                break;
            case ACTION_ENABLE_CPU:
                error = enable_cpu(pm, cpuid);
                break;
            case ACTION_DISABLE_CPU:
                error = disable_cpu(pm, cpuid);
                break;
            case ACTION_SET_MASK:
                break;
            case ACTION_JOIN:
                join = true;
                break;
        }
        if (error) {
            // error ?
        }
    }
    return NULL;
}

int shmem_async_init(pid_t pid, const pm_interface_t *pm, const char *shmem_key) {
    ensure(shmem_cpuinfo__exists() && shmem_procinfo__exists(),
            "shmem_async_init must be called after other shmems initializations"
            " and either cpuinfo or procinfo shmem does not exist");

    // Shared memory creation
    pthread_mutex_lock(&mutex);
    {
        if (shm_handler == NULL) {
            max_helpers = mu_get_system_size();
            shm_handler = shmem_init((void**)&shdata,
                    sizeof(shdata_t) + sizeof(helper_t)*max_helpers, shmem_name, shmem_key);
            subprocesses_attached = 1;
        } else {
            ++subprocesses_attached;
        }
    }
    pthread_mutex_unlock(&mutex);

    helper_t *helper = NULL;
    // Lock shmem to register new subprocess
    shmem_lock(shm_handler);
    {
        int h;
        for (h = 0; h < max_helpers; ++h) {
            // Register helper
            if (shdata->helpers[h].pid == NOBODY) {
                helper = &shdata->helpers[h];

                // Initialize helper structure
                //warning("Initializing helper for pid %d", pid);
                helper->pid = pid;
                helper->pm = pm;
                helper->action = ACTION_NONE;
                sem_init(&helper->sem, /*shared*/ 1, /*value*/ 0);
                break;
            }
        }
    }
    shmem_unlock(shm_handler);

    // return DLB_ERR_NOMEM ?
    fatal_cond(!helper, "no space");

    // is it secure to create out of lock?
    pthread_create(&helper->thread, NULL, thread_start, (void*)helper);

    return DLB_SUCCESS;
}

int shmem_async_finalize(pid_t pid) {
    ensure(shmem_cpuinfo__exists() && shmem_procinfo__exists(),
            "shmem_async_finalize must be called before other shmems finalizations"
            " and either cpuinfo or procinfo shmems does not exist");

    helper_t *helper = NULL;
    // Lock shmem to deregister the subprocess
    shmem_lock(shm_handler);
    {
        helper = get_helper(pid);
        if (helper) {
            // Reset helper fields
            helper->pid = NOBODY;
            helper->cpuid = 0;
            CPU_ZERO(&helper->mask);

            // Join pthread
            helper->action = ACTION_JOIN;
        } else {
            // error ?
        }
    }
    shmem_unlock(shm_handler);

    // return DLB_ERR_NOPROC ?
    fatal_cond(!helper, "no space");

    // is it secure to join out of lock?
    sem_post(&helper->sem);
    pthread_join(helper->thread, NULL);
    sem_destroy(&helper->sem);

    // Shared memory destruction
    pthread_mutex_lock(&mutex);
    {
        if (--subprocesses_attached == 0) {
            shmem_finalize(shm_handler, SHMEM_DELETE);
            shm_handler = NULL;
        }
    }
    pthread_mutex_unlock(&mutex);
    return DLB_SUCCESS;
}

// FIXME: what if helper->action contains a pending action?

void shmem_async_enable_cpu(pid_t pid, int cpuid) {
    helper_t *helper = NULL;
    shmem_lock(shm_handler);
    {
        helper = get_helper(pid);
        if (helper) {
            helper->cpuid = cpuid;
            helper->action = ACTION_ENABLE_CPU;
        }
    }
    shmem_unlock(shm_handler);

    if (helper) {
        sem_post(&helper->sem);
    }
}

void shmem_async_disable_cpu(pid_t pid, int cpuid) {
    helper_t *helper = NULL;
    shmem_lock(shm_handler);
    {
        helper = get_helper(pid);
        if (helper) {
            helper->cpuid = cpuid;
            helper->action = ACTION_DISABLE_CPU;
        }
    }
    shmem_unlock(shm_handler);

    if (helper) {
        sem_post(&helper->sem);
    }
}
