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

#ifndef SHMEM_BITSET_H
#define SHMEM_BITSET_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <sched.h>
#include "support/types.h"

void shmem_bitset__init(const cpu_set_t *cpu_set, const char *shmem_key);
void shmem_bitset__finalize( void );
void shmem_bitset__add_mask( const cpu_set_t *cpu_set );
void shmem_bitset__add_cpu( int cpu );
const cpu_set_t* shmem_bitset__recover_defmask( void );
void shmem_bitset__recover_some_defcpus( cpu_set_t *mask, int max_resources );
int shmem_bitset__return_claimed ( cpu_set_t *mask );
int shmem_bitset__collect_mask(cpu_set_t *mask, int max_resources, priority_t priority);
bool shmem_bitset__is_cpu_borrowed ( int cpu );
bool shmem_bitset__is_cpu_claimed( int cpu );
void shmem_bitset__print_info(const char *shmem_key);
bool shmem_bitset__acquire_cpu( int cpu );
int shmem_bitset__reset_default_cpus(cpu_set_t *mask);

static const struct {
    void (*init)(const cpu_set_t*, const char*);
    void (*finalize) (void);
    void (*add_mask) (const cpu_set_t*);
    void (*add_cpu) (int cpu);
    const cpu_set_t* (*recover_defmask) (void);
    void (*recover_some_defcpus) (cpu_set_t*, int);
    int (*return_claimed) (cpu_set_t*);
    int (*collect_mask)(cpu_set_t*, int, priority_t);
    bool (*is_cpu_borrowed) (int);
    bool (*is_cpu_claimed) (int);
    void (*print_info)(const char*);
    bool (*acquire_cpu) (int);
    int (*reset_default_cpus) (cpu_set_t*);

} shmem_mask = {
    shmem_bitset__init,
    shmem_bitset__finalize,
    shmem_bitset__add_mask,
    shmem_bitset__add_cpu,
    shmem_bitset__recover_defmask,
    shmem_bitset__recover_some_defcpus,
    shmem_bitset__return_claimed,
    shmem_bitset__collect_mask,
    shmem_bitset__is_cpu_borrowed,
    shmem_bitset__is_cpu_claimed,
    shmem_bitset__print_info,
    shmem_bitset__acquire_cpu,
    shmem_bitset__reset_default_cpus
};

#endif /* SHMEM_BITSET_H */
