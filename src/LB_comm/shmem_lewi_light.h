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

#ifndef SHMEM_LEWI_LIGHT_H
#define SHMEM_LEWI_LIGHT_H

void shmem_lewi_light__init(int def_cpus, int is_greedy, const char *shmem_key);
void shmem_lewi_light__finalize(void);

int shmem_lewi_light__release_cpus(int num_cpus);
int shmem_lewi_light__acquire_cpus(int current_cpus);
int shmem_lewi_light__check_idle_cpus(int my_cpus, int max_resources);

void shmem_lewi_light__atfork_prepare(void);
void shmem_lewi_light__atfork_parent(void);
void shmem_lewi_light__atfork_child(void);

#endif /* SHMEM_LEWI_LIGHT_H */

