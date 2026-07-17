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

/* MNGO application manager */

#include "apis/dlb_mngo.h"

#include "apis/dlb_errors.h"
#include "LB_core/spd.h"
#include "mngo/mngo.h"
#include "support/debug.h"
#include "support/dlb_common.h"

/*********************************************************************************/
/*    MNGO                                                                       */
/*********************************************************************************/

DLB_EXPORT_SYMBOL
int DLB_MNGO_Wake_manager(void) {
    spd_enter_dlb(NULL);
    if (unlikely(!thread_spd->mngo_info)) {
      return DLB_ERR_NOMNGO;
    }
    return mngo_manager_wake(thread_spd);
}

/*********************************************************************************/
/*    MNGO Regions                                                               */
/*********************************************************************************/

DLB_EXPORT_SYMBOL
dlb_mngo_region_t* DLB_MNGO_RegionRegister(const char *name) {
  spd_enter_dlb(NULL);
  if (unlikely(!thread_spd->mngo_info)) {
    return NULL;
  }
  return mngo_region_register(thread_spd, name);
}

DLB_EXPORT_SYMBOL
int DLB_MNGO_RegionStart(dlb_mngo_region_t *region) {
  spd_enter_dlb(NULL);
  if (unlikely(!thread_spd->mngo_info)) {
    return DLB_ERR_NOMNGO;
  }
  return mngo_region_start(thread_spd, region);
}

DLB_EXPORT_SYMBOL
int DLB_MNGO_RegionStop(dlb_mngo_region_t *region) {
  spd_enter_dlb(NULL);
  if (unlikely(!thread_spd->mngo_info)) {
    return DLB_ERR_NOMNGO;
  }
  return mngo_region_stop(thread_spd, region);
}
