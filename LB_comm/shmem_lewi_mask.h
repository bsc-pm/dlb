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

#ifndef SHMEM_LEWI_MASK_H
#define SHMEM_LEWI_MASK_H

#define _GNU_SOURCE
#include <sched.h>

const char* mask_to_str ( cpu_set_t *cpu_set );
void shmem_lewi_mask_init( cpu_set_t *cpu_set );
void shmem_lewi_mask_finalize( void );
void shmem_lewi_mask_add_mask( cpu_set_t *cpu_set );
cpu_set_t* shmem_lewi_mask_recover_defmask( void );
int shmem_lewi_mask_collect_mask ( cpu_set_t *cpu_set, int max_resources );

#endif /* SHMEM_LEWI_MASK_H */
