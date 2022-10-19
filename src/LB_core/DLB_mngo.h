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

#ifndef DLB_CORE_MNGO_H
#define DLB_CORE_MNGO_H

#include "../LB_comm/shmem_mngo.h"

/**
 * The size of the MNGO history
 */
#define MNGO_HISTORY_WIDNOW 16

/**
 * This structure holds the history of the application's performance throught
 * the different MNGO samples. As well ass the decisions taken by MNGO.
 *
 * With the intention to use this values for future decisions.
 */
typedef struct {
  /**
   * Circular buffer state
   */
  int head; // The index to the youngest valid value
  int tail; // The index to the oldest valid value

  /**
   * History of enabled dlb modules
   */
  // History of LeWI state throught MNGO samples
  bool lewi[MNGO_HISTORY_WIDNOW];
  // History of DROM state throught MNGO samples
  bool drom[MNGO_HISTORY_WIDNOW];

  /**
   * History of performance
   */
  // History of parallel efficiencies
  float parallel_efficiency[MNGO_HISTORY_WIDNOW];
  // History of load balance
  float load_balance_efficiency[MNGO_HISTORY_WIDNOW];
} mngo_history_t;

/**
 * This structure holds the information necessary for MNGO to work. 
 */
typedef struct DLB_mngo_info_t {
  // The manager id / Or MngoID
  size_t mid;

  // The manager pthread
  pthread_t thread;

  // The TALP monitor necessary for the metric gathering
  struct dlb_monitor_t *monitor; 

  // History of MNGO actions and app performance
  mngo_history_t history;        
} mngo_info_t;

void mngo_init(struct SubProcessDescriptor *spd);

void mngo_fini(struct SubProcessDescriptor *spd);

#endif
