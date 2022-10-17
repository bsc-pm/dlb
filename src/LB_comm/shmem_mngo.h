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

#ifndef SHMEM_MNGO_H
#define SHMEM_MNGO_H

#include <stddef.h>
#include <stdbool.h>

enum {
  DLB_MNGO_SHM_TRY = 0,
  DLB_MNGO_SHM_FORCE = 1
};

/**
 * Data type for MNGO decisions upon itself.
 */
typedef enum {
  MNGO_NONE = 0,
  MNGO_STOP,
  MNGO_ABORT
} mngo_action_t;

/**
 * Data type for MNGO decisions upon other DLB modules.
 */
typedef enum { 
  ACTION_NONE = 0,
  ACTION_ENABLE_MODULE,
  ACTION_DISABLE_MODULE
} mngo_module_action_t;

/**
 * Data type to pack the decisions upon all possible modules. With the intent
 * of communicateing this information througt diferent managers.
 */
typedef struct { 
  mngo_action_t mngo; 
  mngo_module_action_t lewi;
  mngo_module_action_t drom;
} mngo_message_t;

/**
 * Function: shmem_mngo_init
 * -------------------------
 * initializes the shared memory for the MNGO module.
 *
 * shmem_key: it is a unique identifier amongst nodes.
 * mid: is a pointer to an integer where the unique identifier within the node 
 *      will be written.
 *
 * returns: a status of the function, usually DLB_MNGO_SHM_SUCCESS
 *
 */
int shmem_mngo_init(const char *shmem_key, size_t *mid);

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

/**
 * Function: shmem_mngo_send_message
 * ---------------------------------
 * puts the message into the shared memory, at the inbox specified by mid.
 *
 * mid:     the index that identifies the inbox where to place the message
 * message: the message to put to the inbox
 * option:  DLB_MNGO_SHM_TRY to write only if there is no message in the inbox;
 *          and DLB_MNGO_SHM_FORCE to overwrite the message, if it exists.
 *
 * returns: DLB_MNGO_SHM_FULL_INBOX if the inbox is full and option is DLB_MNGO_SHM_TRY, 
 *          DLB_MNGO_SHM_SUCCESS otherwise.
 */
int shmem_mngo_send_message(const size_t mid, const mngo_message_t *message, int options);

/**
 * Function: shmem_mngo_broadcast_message
 * --------------------------------------
 * puts the message into the shared memory to all the active inboxs
 *
 * message: the message to put to the inboxes
 * option:  DLB_MNGO_SHM_TRY to write only if there is no message in any inbox;
 *          and DLB_MNGO_SHM_FORCE to overwrite the message in all inboxes, if any exist.
 *
 * returns: DLB_MNGO_SHM_FULL_INBOX if any inbox is full and option is DLB_MNGO_SHM_TRY, 
 *          DLB_MNGO_SHM_SUCCESS otherwise.
 */
int shmem_mngo_broadcast_message(const mngo_message_t *message, int options);

/**
 * Function: shmem_mngo_broadcast_message
 * --------------------------------------
 * reads the inbox from the shared memory and copies it to message
 *
 * mid:     the inbox from where to read the message
 * message: the message where to put the read message
 *
 * returns: DLB_MNGO_SHM_EMPTY_INBOX if there is no message in the inbox, 
 *          DLB_MNGO_SHM_SUCCESS otherwise.
 */
int shmem_mngo_read_message(const size_t mid, mngo_message_t *message);

#endif /* SHMEM_MNGO_H */
