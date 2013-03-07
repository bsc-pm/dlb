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

#define _GNU_SOURCE
#include <sched.h>
#include <string.h>
#include <limits.h>
#include "support/debug.h"
#include "support/utils.h"
#include "support/mask_utils.h"

#ifdef USE_HWLOC
#include <hwloc.h>
#include <hwloc/bitmap.h>
#include <hwloc/glibc-sched.h>

static bool _socket_aware = true;
static hwloc_topology_t topology = 0;

inline static void topology_init( void )
{
   hwloc_topology_init(&topology);
   hwloc_topology_load(topology);
}

inline static void get_parent_mask_by_obj( cpu_set_t *parent_set, const cpu_set_t *child_set, const hwloc_obj_t obj, mu_opt_t condition )
{
   cpu_set_t intxn;
   cpu_set_t obj_mask;
   hwloc_cpuset_to_glibc_sched_affinity( topology, obj->cpuset, &obj_mask, sizeof(cpu_set_t) );
   CPU_AND( &intxn, &obj_mask, child_set );

   if ( (condition == MU_ANY_BIT && CPU_COUNT( &intxn ) > 0) ||
        (condition == MU_ALL_BITS && CPU_COUNT( &intxn ) == CPU_COUNT( &obj_mask )  ) ) {
      CPU_OR( parent_set, parent_set, &obj_mask );
   }
}
#endif

void mu_get_parent_mask( cpu_set_t *parent_set, const cpu_set_t *child_set, mu_opt_t condition )
{
#ifdef USE_HWLOC
   if ( !topology ) topology_init();

   if ( _socket_aware ) {
      hwloc_obj_t numa_node = hwloc_get_obj_by_type( topology, HWLOC_OBJ_NODE, 0 );
      if ( numa_node )  {
         // For each NUMA node:
         for ( ; numa_node; numa_node = numa_node->next_sibling ) {
            get_parent_mask_by_obj( parent_set, child_set, numa_node, condition );
         }
      } else {
         // For each socket (only if there's no NUMA nodes)
         hwloc_obj_t socket = hwloc_get_obj_by_type( topology, HWLOC_OBJ_SOCKET, 0 );
         for ( ; socket; socket = socket->next_sibling ) {
            get_parent_mask_by_obj( parent_set, child_set, socket, condition );
         }
      }
   } else {
      hwloc_obj_t machine = hwloc_get_obj_by_type( topology, HWLOC_OBJ_MACHINE, 0 );
      hwloc_cpuset_to_glibc_sched_affinity( topology, machine->cpuset, parent_set, sizeof(cpu_set_t) );
   }
#else
   memset( parent_set, INT_MAX, sizeof(cpu_set_t) );
#endif
}
