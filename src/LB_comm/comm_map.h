/*************************************************************************************/
/*      Copyright 2012 Barcelona Supercomputing Center                               */
/*                                                                                   */
/*      This file is part of the DLB library.                                        */
/*                                                                                   */
/*      DLB is free software: you can redistribute it and/or modify                  */
/*      it under the terms of the GNU Lesser General Public License as published by  */
/*      the Free Software Foundation, either version 3 of the License, or            */
/*      (at your option) any later version.                                          */
/*                                                                                   */
/*      DLB is distributed in the hope that it will be useful,                       */
/*      but WITHOUT ANY WARRANTY; without even the implied warranty of               */
/*      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                */
/*      GNU Lesser General Public License for more details.                          */
/*                                                                                   */
/*      You should have received a copy of the GNU Lesser General Public License     */
/*      along with DLB.  If not, see <http://www.gnu.org/licenses/>.                 */
/*************************************************************************************/

#ifndef COMM_LEND_LIGHT_H
#define COMM_LEND_LIGHT_H

void ConfigShMem_Map(int num_procs, int meId, int nodeId, int defCPUS, int *my_cpus);

int releaseCpus_Map(int cpus, int*released_cpus);

int acquireCpus_Map(int current_cpus, int* new_cpus);

int checkIdleCpus_Map(int myCpus, int max_cpus, int* new_cpus);

void finalize_comm_Map();

#endif //COMM_LEND_LIGHT

