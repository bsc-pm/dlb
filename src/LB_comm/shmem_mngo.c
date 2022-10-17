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

#include "LB_comm/shmem.h"
#include "LB_comm/shmem_mngo.h"

#include "support/debug.h"
#include "support/mask_utils.h"

#include "apis/dlb_errors.h"

#include <stdio.h>

/**
 * The struct containing the message if any of a manager.
 */
typedef struct {
  bool active;
  bool pending;
  mngo_message_t message;
} mngo_message_inbox_t;


/**
 * The data structure of the MNGO shared memory.
 */
typedef struct {
  mngo_message_inbox_t inbox[0];
} shdata_t ;

/**
 * The pointer to the data structure used by MNGO for the communication among
 * the different manager threads.
 */
static shdata_t *shdata = NULL;

/**
 * The pointer to the shm handler to interact with the shmem interface.
 */
static shmem_handler_t *shm_handler = NULL;

/**
 * The maximum of sub processes that can be atached to the shared memory.
 */
static size_t max_atached;

/**
 * The amount of sub processes atached to the shared memory.
 */
static size_t subprocesses_attached;

/**
 * The shared memory name. Used to identify shared memory files for the MNGo module.
 */
static const char *mngo_shmem_name = "mngo";

/**
 * Process wide pthread mutex.
 */
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

// static void cleanup_mngo_shmem(void *shdata_ptr, int pid){ }

static void close_shmem() {
  subprocesses_attached--;
  if (subprocesses_attached == 0) {
    shmem_finalize(shm_handler, NULL /* do not check if empty */);
  }
}

int shmem_mngo_init(const char *shmem_key, size_t *mid) {

  int error = DLB_ERR_UNKNOWN;

  pthread_mutex_lock(&mutex);
  {
    if (shm_handler == NULL) {
      max_atached = mu_get_system_size();
      shm_handler = shmem_init(
          (void **)&shdata,
          sizeof(shdata_t) + sizeof(mngo_message_t) * max_atached,
          mngo_shmem_name, shmem_key, SHMEM_VERSION_IGNORE, NULL);
      subprocesses_attached = 1;
    } else {
      subprocesses_attached++;
    }
  }
  pthread_mutex_unlock(&mutex);

  size_t id;
  shmem_lock(shm_handler);
  {
    /* If it is the first process to access this the whole shm will be 0 */
    for (id = 0; id < max_atached; id++) {
      if (! shdata->inbox[id].active) {
        shdata->inbox[id].active = true;
        error = DLB_SUCCESS;
        break;
      }
    }
  }
  shmem_unlock(shm_handler);

  if (id == max_atached) {
    error = DLB_ERR_NOSHMEM;
  }

  if (error != DLB_SUCCESS) {
    pthread_mutex_lock(&mutex);
    close_shmem();
    pthread_mutex_unlock(&mutex);
    warn_error(error);
  }

  *mid = id;

  return error;
}

int shmem_mngo_fini(size_t mid) {

  if (shdata->inbox[mid].active) {
    pthread_mutex_lock(&mutex);
    {
      bool was_active;
      shmem_lock(shm_handler);
      {
        was_active = shdata->inbox[mid].active;
        shdata->inbox[mid].active = false;
      }
      shmem_unlock(shm_handler);

      if (was_active) {
        close_shmem();
      }
    }
    pthread_mutex_unlock(&mutex);
  }

  return DLB_SUCCESS;
}

int shmem_mngo_send_message(const size_t mid, const mngo_message_t *message,
                            int options) {

  int error = DLB_ERR_UNKNOWN;

  shmem_lock(shm_handler);
  {
    if (mid > max_atached || !shdata->inbox[mid].active) {
      fprintf(stderr, "MNGO Shm: Can not read message from unitialized MNGO "
                      "shmem inbox.\n");
      error = DLB_ERR_UNKNOWN; // TODO Change for something similar to internal
                               // error
      goto send_error;
    }

    if (options != DLB_MNGO_SHM_FORCE && shdata->inbox[mid].pending) {
      // error = FULL INBOX; TODO Create an error for full inbox.
      goto send_error;
    }

    shdata->inbox[mid].pending = true;
    shdata->inbox[mid].message = *message;
    error = DLB_SUCCESS;
  }
send_error:
  shmem_unlock(shm_handler);
  return error;
}

int shmem_mngo_broadcast_message(const mngo_message_t *message, int options) {

  int error = DLB_ERR_UNKNOWN;

  shmem_lock(shm_handler);
  {
    if (options != DLB_MNGO_SHM_FORCE) {
      size_t mid;
      for (mid = 0; mid < max_atached; mid++) {
        if (shdata->inbox[mid].active && shdata->inbox[mid].pending) {
          // error = FULL INBOX TODO Create an error for full inbox.
          goto broadcast_error;
        }
      }
    }

    size_t mid;
    for (mid = 0; mid < max_atached; mid++) {
      if (shdata->inbox[mid].active) {
        shdata->inbox[mid].pending = true;
        shdata->inbox[mid].message = *message;
      }
    }
    error = DLB_SUCCESS;
  }
broadcast_error:
  shmem_unlock(shm_handler);
  return error;
}

int shmem_mngo_read_message(const size_t mid, mngo_message_t *message) {

  int error = DLB_ERR_UNKNOWN;

  shmem_lock(shm_handler);
  {
    if (mid > max_atached || !shdata->inbox[mid].active) {
      // TODO print a message error with the DLB interface
      fprintf(stderr, "MNGO Shm: Can not read message from unitialized MNGO "
                      "shmem inbox.\n");
      goto read_error;
    }

    if (!shdata->inbox[mid].pending) {
      // error = EMPTY INBOX // TODO Create an error for empty inbox.
      goto read_error;
    }

    *message = shdata->inbox[mid].message;

    /**
     * Reset the message.
     */
    shdata->inbox[mid].pending = false;
    shdata->inbox[mid].message = (const mngo_message_t){
        .mngo = MNGO_NONE,
        .lewi = ACTION_NONE,
        .drom = ACTION_NONE,
    };
    error = DLB_SUCCESS;
  }
read_error:
  shmem_unlock(shm_handler);
  return error;
}
