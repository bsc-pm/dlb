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

#include "LB_core/spd.h"
#include "LB_core/DLB_kernel.h"
#include "LB_core/DLB_talp.h"
#include "LB_core/DLB_mngo.h"

#include "apis/dlb_talp.h"
#include "apis/dlb_errors.h"

#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

static void mngo_gather_node_metrics(struct SubProcessDescriptor *spd,
                                  dlb_node_metrics_t *node_metrics) {
  mngo_info_t *mngo_info = spd->mngo_info;
  monitoring_region_stop(spd, mngo_info->monitor);
  talp_collect_node_metrics(spd, mngo_info->monitor, node_metrics);
  monitoring_region_reset(mngo_info->monitor);
  monitoring_region_start(spd, mngo_info->monitor);
}

static void mngo_action_activate_lewi(struct SubProcessDescriptor *spd) {
  set_dlb_enabled(spd, true);
  // spd->lb_funcs.enable(spd);
  // instrument_event(DLB_MODE_EVENT, EVENT_DISABLED, EVENT_END);
}

static void mngo_action_deactivate_lewi(struct SubProcessDescriptor *spd) {
  set_dlb_enabled(spd, false);
  // spd->lb_funcs.disable(spd);
  // instrument_event(DLB_MODE_EVENT, EVENT_DISABLED, EVENT_BEGIN);
}

static void mngo_action_activate_drom(struct SubProcessDescriptor *spd) {}
static void mngo_action_deactivate_drom(struct SubProcessDescriptor *spd) {}

static void mngo_decide_actions(struct SubProcessDescriptor *spd,
                                const dlb_node_metrics_t *node_metrics,
                                mngo_message_t *message) {

  *message = (const mngo_message_t){
      .mngo = MNGO_NONE,
      .lewi = ACTION_NONE,
      .drom = ACTION_NONE,
  };

  // TODO replace this by a parametrized value or some other logic
  if (node_metrics->load_balance < 80) {
    message->lewi = ACTION_ENABLE_MODULE;
  }
}

static void mngo_commit_actions(struct SubProcessDescriptor *spd,
                         const mngo_message_t *message) {

  if (message->lewi == ACTION_ENABLE_MODULE) {
    mngo_action_activate_lewi(spd);
  } else if (message->lewi == ACTION_DISABLE_MODULE) {
    mngo_action_deactivate_lewi(spd);
  }

  if (message->drom == ACTION_ENABLE_MODULE) {
    mngo_action_activate_drom(spd);
  } else if (message->drom == ACTION_DISABLE_MODULE) {
    mngo_action_deactivate_drom(spd);
  }
}

static void mngo_update_history(struct SubProcessDescriptor *spd,
                                const dlb_node_metrics_t *node_metrics,
                                const mngo_message_t *message) {

  mngo_info_t  *mngo_info = (mngo_info_t *)spd->mngo_info;

  int old_head = mngo_info->history.head;
  mngo_info->history.head = (mngo_info->history.head + 1) % MNGO_HISTORY_WIDNOW;

  // If the head catches the tail, advance tail
  if (mngo_info->history.head == mngo_info->history.tail) {
    mngo_info->history.tail =
        (mngo_info->history.tail + 1) % MNGO_HISTORY_WIDNOW;
  }

  mngo_info->history.parallel_efficiency[mngo_info->history.head] =
      node_metrics->parallel_efficiency;

  mngo_info->history.load_balance_efficiency[mngo_info->history.head] =
      node_metrics->load_balance;

  switch (message->lewi) {
  case ACTION_ENABLE_MODULE:
    mngo_info->history.lewi[mngo_info->history.head] = true;
    break;
  case ACTION_DISABLE_MODULE:
    mngo_info->history.lewi[mngo_info->history.head] = false;
    break;
  default:
    mngo_info->history.lewi[mngo_info->history.head] =
        mngo_info->history.lewi[old_head];
    break;
  };

  switch (message->drom) {
  case ACTION_ENABLE_MODULE:
    mngo_info->history.lewi[mngo_info->history.head] = true;
    break;
  case ACTION_DISABLE_MODULE:
    mngo_info->history.lewi[mngo_info->history.head] = false;
    break;
  default:
    mngo_info->history.lewi[mngo_info->history.head] =
        mngo_info->history.lewi[old_head];
    break;
  };
}

static void mngo_manager(struct SubProcessDescriptor *spd) {

  mngo_info_t *mngo_info = (mngo_info_t*) spd->mngo_info;

  while (1) {
    /**
     * Only the node representative manager gathers the metrics, takes decisions
     * and sends them to the other managers.
     */
    if (mngo_info->mid == 0) {

      /**
       * Time between MNGO managment events.
       */
      sleep(30); // TODO: replace this by a parametrized value

      dlb_node_metrics_t node_metrics;
      mngo_gather_node_metrics(spd, &node_metrics);

      mngo_message_t message;
      mngo_decide_actions(spd, &node_metrics, &message);

      mngo_update_history(spd, &node_metrics, &message);

      int status = shmem_mngo_broadcast_message(&message, DLB_MNGO_SHM_TRY);
      
      /**
       * If some inbox is full skip.
       */
      if (status == DLB_ERR_UNKNOWN) {
        continue;
      }
    }

    /**
     * After the representative manager wakes up and runs de decision making.
     * The barrier is complete and all the managers can continue to read and
     * execute de decisions.
     */
    // shmem_barrier(); TODO find out how to use the shmem_barrier.h interface
    {
      mngo_message_t message;
      shmem_mngo_read_message(mngo_info->mid, &message);

      mngo_commit_actions(spd, &message);

      if (message.mngo != MNGO_NONE) {
        shmem_mngo_fini(mngo_info->mid);
        break;
      }
    }
  }
  // Finalize thread
}

void mngo_init(struct SubProcessDescriptor *spd) {

  mngo_info_t *mngo_info = (mngo_info_t *)calloc(1, sizeof(mngo_info_t));

  /**
   * Initialize this process manegers shared memory.
   */
  if (shmem_mngo_init(spd->options.shm_key, &mngo_info->mid) != DLB_SUCCESS) {
    // DO SOMETHING
  }

  /**
   * Initialize the TALP monitoring regions for MNGO.
   */
  mngo_info->monitor = monitoring_region_register("MNGO");

  /**
   * Initialize the MNGO manager thread for this process.
   */
  pthread_t thread;
  pthread_create(&thread, NULL, (void * (*)(void*)) mngo_manager, (void*) spd);
  mngo_info->thread = thread;

  /**
   * Store the MNGO info to the SubProcessDescriptor.
   */
  spd->mngo_info = mngo_info;
}

void mngo_fini(struct SubProcessDescriptor *spd) {

  mngo_info_t *mngo_info = spd->mngo_info;

  if (true /* TODO mngo_info->shmem_active */) {
    /**
     * Send the stop message throug the shm and signal the manager thread to wake up.
     */
    const mngo_message_t message = (const mngo_message_t){
        .mngo = MNGO_STOP,
        .lewi = ACTION_NONE,
        .drom = ACTION_NONE,
    };
    shmem_mngo_send_message(mngo_info->mid, &message, DLB_MNGO_SHM_FORCE);
    // TODO insert here the mechanism to wake up threads
  }

  /**
   * Join with the manager thread.
   */
  int *retval;
  pthread_join(mngo_info->thread, (void**)&retval);

  /**
   * Free the TALP monitoring region.
   */
  monitoring_region_stop(spd, mngo_info->monitor);

  /**
   * Free the mngo_info_t struct from the sub-process descriptor.
   */
  free(mngo_info);
}
