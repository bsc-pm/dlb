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

#ifndef SHMEM_BITSET_H
#define SHMEM_BITSET_H

#define _GNU_SOURCE
#include <sched.h>
#include <stdbool.h>

void shmem_bitset__init( const cpu_set_t *cpu_set );
void shmem_bitset__finalize( void );
void shmem_bitset__add_mask( const cpu_set_t *cpu_set );
const cpu_set_t* shmem_bitset__recover_defmask( void );
void shmem_bitset__recover_some_defcpus( cpu_set_t *mask, int max_resources );
int shmem_bitset__return_claimed ( cpu_set_t *mask );
int shmem_bitset__collect_mask ( cpu_set_t *mask, int max_resources );
bool shmem_bitset__is_cpu_borrowed ( int cpu );
bool shmem_bitset__is_cpu_claimed( int cpu );
void shmem_bitset__print_info( void );

static const struct {
   void (*init) (const cpu_set_t *cpu_set);
   void (*finalize) (void);
   void (*add_mask) (const cpu_set_t*);
   const cpu_set_t* (*recover_defmask) (void);
   void (*recover_some_defcpus) (cpu_set_t*, int);
   int (*return_claimed) (cpu_set_t*);
   int (*collect_mask) (cpu_set_t*, int);
   bool (*is_cpu_borrowed) (int);
   bool (*is_cpu_claimed) (int);
   void (*print_info) (void);
} shmem_mask = {
   shmem_bitset__init,
   shmem_bitset__finalize,
   shmem_bitset__add_mask,
   shmem_bitset__recover_defmask,
   shmem_bitset__recover_some_defcpus,
   shmem_bitset__return_claimed,
   shmem_bitset__collect_mask,
   shmem_bitset__is_cpu_borrowed,
   shmem_bitset__is_cpu_claimed,
   shmem_bitset__print_info
};

#endif /* SHMEM_BITSET_H */