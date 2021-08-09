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

enum { NOBODY = 0 };
enum { QUEUE_SIZE = 100 };

typedef enum HelperAction {
    ACTION_NONE = 0,
    ACTION_ENABLE_CPU,
    ACTION_DISABLE_CPU,
    ACTION_SET_MASK,
    ACTION_JOIN
} action_t;

typedef struct Message {
    action_t action;
    int cpuid;
} message_t;

typedef struct {
    /* Queue attributes */
    message_t       queue[QUEUE_SIZE];
    unsigned int    q_head;
    unsigned int    q_tail;
    pthread_mutex_t q_lock;
    pthread_cond_t  q_wait_data;

    /* Helper metadata */
    pid_t pid;
    pthread_t pth;
    cpu_set_t mask;
    const pm_interface_t *pm;
    bool joinable;
} helper_t;


typedef struct {
    helper_t helpers[0];
} shdata_t;

enum { SHMEM_ASYNC_VERSION = 2 };

static int max_helpers = 0;
static shdata_t *shdata = NULL;
static const char *shmem_name = "async";
static shmem_handler_t *shm_handler = NULL;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static int subprocesses_attached = 0;

static helper_t* get_helper(pid_t pid) {
    int h;
    for (h = 0; shdata && h < max_helpers; ++h) {
        if (shdata->helpers[h].pid == pid) {
            return &shdata->helpers[h];
        }
    }
    return NULL;
}

static void enqueue_message(helper_t *helper, const message_t *message) {
    /* Discard message if helper does not accept new inputs */
    if (helper->joinable) {
        return;
    }

    pthread_mutex_lock(&helper->q_lock);

    /* If ACTION_JOIN, flag helper to reject further messages */
    if (message->action == ACTION_JOIN) {
        helper->joinable = true;
    }

    /* Get next_head index and check that buffer is not full */
    unsigned int next_head = (helper->q_head + 1) % QUEUE_SIZE;
    if (__builtin_expect((next_head == helper->q_tail), 0)) {
        pthread_mutex_unlock(&helper->q_lock);
        fatal("Max petitions requested for asynchronous thread");
    }

    /* Enqueue message and update head */
    verbose(VB_ASYNC, "Writing message %d, head is now %d", helper->q_head, next_head);
    helper->queue[helper->q_head] = *message;
    helper->q_head = next_head;

    /* Signal helper */
    pthread_cond_signal(&helper->q_wait_data);

    pthread_mutex_unlock(&helper->q_lock);
}

static void dequeue_message(helper_t *helper, message_t *message) {
    pthread_mutex_lock(&helper->q_lock);

    /* Block until there's some message in the queue */
    while (helper->q_head == helper->q_tail) {
        pthread_cond_wait(&helper->q_wait_data, &helper->q_lock);
    }

    /* Dequeue message and update tail */
    unsigned int next_tail = (helper->q_tail + 1) % QUEUE_SIZE;
    verbose(VB_ASYNC, "Reading message %d, tail is now %d", helper->q_tail, next_tail);
    *message = helper->queue[helper->q_tail];
    helper->q_tail = next_tail;

    pthread_mutex_unlock(&helper->q_lock);
}

static void* thread_start(void *arg) {
    helper_t *helper = arg;
    const pm_interface_t* const pm = helper->pm;
    pthread_setaffinity_np(helper->pth, sizeof(cpu_set_t), &helper->mask);
    verbose(VB_ASYNC, "Helper thread started, pinned to %s", mu_to_str(&helper->mask));

    bool join = false;
    while(!join || helper->q_head != helper->q_tail) {
        message_t message;
        dequeue_message(helper, &message);
        verbose(VB_ASYNC, "Helper thread attending petition %d", message.action);

        int error = 0;
        switch(message.action) {
            case ACTION_NONE:
                break;
            case ACTION_ENABLE_CPU:
                error = enable_cpu(pm, message.cpuid);
                break;
            case ACTION_DISABLE_CPU:
                error = disable_cpu(pm, message.cpuid);
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
    verbose(VB_ASYNC, "Helper thread finalizing");
    return NULL;
}

static void cleanup_shmem(void *shdata_ptr, int pid) {
    shdata_t *shared_data = shdata_ptr;
    int h;
    for (h = 0; h < max_helpers; ++h) {
        if (shared_data->helpers[h].pid == pid) {
            shared_data->helpers[h] = (const helper_t){{{0}}};
        }
    }
}

int shmem_async_init(pid_t pid, const pm_interface_t *pm, const cpu_set_t *process_mask,
        const char *shmem_key) {
    verbose(VB_ASYNC, "Creating helper thread");

    // Shared memory creation
    pthread_mutex_lock(&mutex);
    {
        if (shm_handler == NULL) {
            max_helpers = mu_get_system_size();
            shm_handler = shmem_init((void**)&shdata,
                    sizeof(shdata_t) + sizeof(helper_t)*max_helpers,
                    shmem_name, shmem_key, SHMEM_ASYNC_VERSION, cleanup_shmem);
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

                /* Initialize queue structure */
                memset(helper->queue, 0, sizeof(helper->queue));
                helper->q_head = 0;
                helper->q_tail = 0;

                /* Initialize queue sync attributes */
                pthread_mutexattr_t mutex_attr;
                fatal_cond_strerror( pthread_mutexattr_init(&mutex_attr) );
                fatal_cond_strerror( pthread_mutexattr_setpshared(&mutex_attr,
                            PTHREAD_PROCESS_SHARED) );
                fatal_cond_strerror( pthread_mutex_init(&helper->q_lock, &mutex_attr) );
                fatal_cond_strerror( pthread_mutexattr_destroy(&mutex_attr) );
                pthread_condattr_t cond_attr;
                fatal_cond_strerror( pthread_condattr_init(&cond_attr) );
                fatal_cond_strerror( pthread_condattr_setpshared(&cond_attr,
                            PTHREAD_PROCESS_SHARED) );
                fatal_cond_strerror( pthread_cond_init(&helper->q_wait_data, &cond_attr) );
                fatal_cond_strerror( pthread_condattr_destroy(&cond_attr) );

                // Initialize helper metadata and create thread
                helper->pm = pm;
                helper->pid = pid;
                memcpy(&helper->mask, process_mask, sizeof(cpu_set_t));
                pthread_create(&helper->pth, NULL, thread_start, (void*)helper);
                break;
            }
        }
    }
    shmem_unlock(shm_handler);

    return helper ? DLB_SUCCESS : DLB_ERR_NOMEM;
}

int shmem_async_finalize(pid_t pid) {
    verbose(VB_ASYNC, "Finalizing helper thread for pid: %d", pid);
    helper_t *helper = get_helper(pid);
    if (helper) {
        /* Enqueue JOIN message  */
        message_t message = { .action = ACTION_JOIN };
        enqueue_message(helper, &message);

        /* Wait helper thread to finish */
        pthread_join(helper->pth, NULL);
        verbose(VB_ASYNC, "Helper thread joined");

        /* Clear helper data */
        shmem_lock(shm_handler);
        {
            pthread_mutex_destroy(&helper->q_lock);
            pthread_cond_destroy(&helper->q_wait_data);
            memset(helper, 0, sizeof(*helper));
        }
        shmem_unlock(shm_handler);

        /* Shared memory destruction */
        pthread_mutex_lock(&mutex);
        {
            if (--subprocesses_attached == 0) {
                shmem_finalize(shm_handler, SHMEM_DELETE);
                shm_handler = NULL;
                shdata = NULL;
            }
        }
        pthread_mutex_unlock(&mutex);
    }

    return helper ? DLB_SUCCESS : DLB_ERR_NOPROC;
}


void shmem_async_enable_cpu(pid_t pid, int cpuid) {
    verbose(VB_ASYNC, "Enqueuing petition for pid: %d, enable cpuid %d", pid, cpuid);
    helper_t *helper = get_helper(pid);
    if (helper) {
        message_t message = { .action = ACTION_ENABLE_CPU, .cpuid = cpuid };
        enqueue_message(helper, &message);
    }
}

void shmem_async_disable_cpu(pid_t pid, int cpuid) {
    verbose(VB_ASYNC, "Enqueuing petition for pid: %d, disable cpuid %d", pid, cpuid);
    helper_t *helper = get_helper(pid);
    if (helper) {
        message_t message = { .action = ACTION_DISABLE_CPU, .cpuid = cpuid };
        enqueue_message(helper, &message);
    }
}

int shmem_async__version(void) {
    return SHMEM_ASYNC_VERSION;
}
size_t shmem_async__size(void) {
    return sizeof(shdata_t) + sizeof(helper_t)*mu_get_system_size();
}
