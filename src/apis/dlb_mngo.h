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

#ifndef DLB_API_MNGO_H
#define DLB_API_MNGO_H

/*********************************************************************************/
/*    MNGO                                                                       */
/*********************************************************************************/

int DLB_MNGO_Wake_manager(void);

int DLB_MNGO_Enable_LeWI(void);

int DLB_MNGO_Poll_DROM(void);

/*********************************************************************************/
/*    MNGO Regions                                                               */
/*********************************************************************************/

typedef struct dlb_mngo_region_t dlb_mngo_region_t;

typedef enum dlb_mngo_region_flag_t {
    DLB_LEWI_REGION = 1 << 0,
    DLB_DROM_REGION = 1 << 1,
} dlb_mngo_region_flags_t;

dlb_mngo_region_t* DLB_MNGO_RegionRegister(const char *name);

int DLB_MNGO_RegionStart(dlb_mngo_region_t *region);

int DLB_MNGO_RegionStop(dlb_mngo_region_t *region);

#endif /* DLB_API_MNGO_H */


