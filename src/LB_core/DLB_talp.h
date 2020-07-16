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

#ifndef DLB_CORE_TALP_H
#define DLB_CORE_TALP_H

struct SubProcessDescriptor;
struct dlb_monitor_t;

/*  Initializes the module structures */
void talp_init(struct SubProcessDescriptor *spd);

/*  Finalizes the execution of the module and shows the final report if needed */
void talp_finalize(struct SubProcessDescriptor *spd);

/* Start MPI monitoring region */
void talp_mpi_init(void);

/* Stop MPI monitoring region */
void talp_mpi_finalize(void);

/*  Enables the cpuid for the current process */
void talp_cpu_enable(int cpuid);

/*  Disables the cpuid for the current process */
void talp_cpu_disable(int cpuid);

/*  Update the metrics when entering MPI */
void talp_in_mpi(void);

/*  Update the metrics when going out MPI */
void talp_out_mpi(void);

struct dlb_monitor_t* monitoring_region_register(const char* name);
int monitoring_region_reset(struct dlb_monitor_t *monitor);
int monitoring_region_start(struct dlb_monitor_t *monitor);
int monitoring_region_stop(struct dlb_monitor_t *monitor);
int monitoring_region_report(struct dlb_monitor_t *monitor);

#endif
