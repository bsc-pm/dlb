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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include "support/debug.h"
#include "support/globals.h"
#include "support/utils.h"
#include "support/mask_utils.h"

#ifdef HAVE_HWLOC
#include <hwloc.h>
#include <hwloc/bitmap.h>
#include <hwloc/glibc-sched.h>
#endif

typedef struct {
   int size;
   int num_parents;
   cpu_set_t *parents;
   cpu_set_t sys_mask;
} mu_system_loc_t;

static mu_system_loc_t sys;

#if defined HAVE_HWLOC
static void parse_hwloc( void )
{
   hwloc_topology_t topology;
   hwloc_topology_init( &topology );
   hwloc_topology_load( topology );

   hwloc_obj_t obj;
   int num_nodes = hwloc_get_nbobjs_by_type( topology, HWLOC_OBJ_NODE );
   if ( num_nodes > 0 ) {
      sys.num_parents = num_nodes;
      obj = hwloc_get_obj_by_type( topology, HWLOC_OBJ_NODE, 0 );
   } else {
      sys.num_parents = hwloc_get_nbobjs_by_type( topology, HWLOC_OBJ_SOCKET );
      obj = hwloc_get_obj_by_type( topology, HWLOC_OBJ_SOCKET, 0 );
   }

   sys.parents = malloc( sys.num_parents * sizeof(cpu_set_t) );

   int i = 0;
   for ( ; obj; obj = obj->next_sibling ) {
      hwloc_cpuset_to_glibc_sched_affinity( topology, obj->cpuset, &(sys.parents[i++]), sizeof(cpu_set_t) );
   }

   hwloc_obj_t machine = hwloc_get_obj_by_type( topology, HWLOC_OBJ_MACHINE, 0 );
   hwloc_cpuset_to_glibc_sched_affinity( topology, machine->cpuset, &(sys.sys_mask), sizeof(cpu_set_t) );
   sys.size = hwloc_bitmap_weight( machine->cpuset );

   hwloc_topology_destroy( topology );
}
#elif defined IS_BGQ_MACHINE
static void set_bgq_info( void )
{
   sys.size = 64;
   sys.num_parents = 1;
   sys.parents = (cpu_set_t *) malloc( sizeof(cpu_set_t) );
   CPU_ZERO( &(sys.parents[0]) );
   CPU_ZERO( &sys.sys_mask );
   int i;
   for ( i=0; i<64; i++ ) {
      CPU_SET( i, &(sys.parents[0]) );
      CPU_SET( i, &sys.sys_mask );
   }
}
#else
static void parse_lscpu( void )
{
   FILE *pipe = NULL;
   char *line = NULL;
   char *token, *endptr;
   size_t len = 0;
   int cpu, socket, node, id;
   int i;

   pipe = popen( "lscpu -p", "r" );
   if ( pipe == NULL ) {
      perror( "Can't open pipe to lscpu command" );
      exit( EXIT_FAILURE );
   }

   while ( getline( &line, &len, pipe ) != -1 ) {
      if ( !isdigit( line[0] ) ) continue;

      cpu = strtol( line, &endptr, 10 );     /* CPU token */

      token = endptr+1;
      strtol( token, &endptr, 10);           /* Core token, returned value ignored */

      token = endptr+1;
      socket = strtol( token, &endptr, 10);  /* Socket token */

      token = endptr+1;
      node = strtol( token, &endptr, 10 );   /* Node token */

      /* Did lscpu give us a valid node? Otherwise socket id will be used */
      id = (endptr == token) ? socket : node;

      /* realloc array of cpu_set_t's ? */
      if ( id >= sys.num_parents ) {

         sys.parents = realloc( sys.parents, (id+1) * sizeof(cpu_set_t) );
         for ( i=sys.num_parents; i<id+1; i++ ) {
            CPU_ZERO( &(sys.parents[i]) );
         }
         sys.num_parents = id+1;
      }
      CPU_SET( cpu, &(sys.parents[id]) );
   }

   CPU_ZERO( &(sys.sys_mask) );
   for ( i=0; i<sys.num_parents; i++ ) {
      CPU_OR( &(sys.sys_mask), &(sys.sys_mask), &(sys.parents[i]) );
   }
   sys.size = CPU_COUNT( &(sys.sys_mask) );

   free( line );
   pclose( pipe );
}
#endif /* HAVE_HWLOC */

void mu_init( void )
{
   sys.num_parents = 0;
   sys.parents = NULL;

#if defined HAVE_HWLOC
   parse_hwloc();
#elif defined IS_BGQ_MACHINE
   set_bgq_info();
#else
   parse_lscpu();
#endif
}

void mu_finalize( void )
{
   free(sys.parents);
}

int mu_get_system_size( void )
{
   return sys.size;
}

/* Returns the set of parent's masks (aka: socket masks) for the given child_set being condition:
 * MU_ANY_BIT: the intersection between the socket and the child_set must be non-empty
 * MU_ALL_BITS: the socket mask must be a subset of child_set
 */
void mu_get_affinity_mask( cpu_set_t *affinity_set, const cpu_set_t *child_set, mu_opt_t condition )
{
   CPU_ZERO( affinity_set );
   cpu_set_t intxn;
   int i;
   for ( i=0; i<sys.num_parents; i++ ) {
      CPU_AND( &intxn, &(sys.parents[i]), child_set );
      if ( (condition == MU_ANY_BIT && CPU_COUNT( &intxn ) > 0) ||                     /* intxn non-empty */
           (condition == MU_ALL_BITS && CPU_EQUAL( &intxn, &(sys.parents[i]) )) ) {    /* subset ? */
         CPU_OR( affinity_set, affinity_set, &(sys.parents[i]) );
      }
   }
}

const char* mu_to_str ( const cpu_set_t *cpu_set )
{
   int i;
   static char str[CPU_SETSIZE*2 + 4 + 6];
   char str_i[3];
   strcpy( str, "[ " );
   for ( i=0; i<sys.size; i++ ) {
      if ( CPU_ISSET(i, cpu_set) ){
         snprintf(str_i, sizeof(str_i), "%d ", i);
         strcat( str, str_i );
      } else strcat( str,"- " );
   }
   strcat( str, "]\0" );
   return str;
}
