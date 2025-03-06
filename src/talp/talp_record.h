/*********************************************************************************/
/*  Copyright 2009-2025 Barcelona Supercomputing Center                          */
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

#ifndef TALP_RECORD_H
#define TALP_RECORD_H

typedef struct dlb_monitor_t dlb_monitor_t;
typedef struct SubProcessDescriptor subprocess_descriptor_t;

void talp_record_monitor(const subprocess_descriptor_t *spd,
        const dlb_monitor_t *monitor);

#if MPI_LIB

void talp_record_node_summary(const subprocess_descriptor_t *spd);

void talp_record_process_summary(const subprocess_descriptor_t *spd,
        const dlb_monitor_t *monitor);

void talp_record_pop_summary(const subprocess_descriptor_t *spd,
        const dlb_monitor_t *monitor);

#endif

#endif /* TALP_RECORD_H */
