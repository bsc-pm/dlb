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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "support/mask_utils.h"

#include "support/debug.h"

#ifdef HWLOC_LIB
#include <hwloc.h>
#include <hwloc/bitmap.h>
#include <hwloc/glibc-sched.h>
#endif

#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include <sys/types.h>
#include <regex.h>

typedef struct {
    int size;
    int num_parents;
    cpu_set_t *parents;
    cpu_set_t sys_mask;
} mu_system_loc_t;

static mu_system_loc_t sys;
static bool mu_initialized = false;

#if defined HWLOC_LIB
static void parse_hwloc( void ) {
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
static void set_bgq_info( void ) {
    sys.size = 64;
    sys.num_parents = 1;
    sys.parents = malloc( sizeof(cpu_set_t) );
    CPU_ZERO( &(sys.parents[0]) );
    CPU_ZERO( &sys.sys_mask );
    int i;
    for ( i=0; i<64; i++ ) {
        CPU_SET( i, &(sys.parents[0]) );
        CPU_SET( i, &sys.sys_mask );
    }
}
#else
static void parse_lscpu( void ) {
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
        if ( !isdigit( line[0] ) ) { continue; }

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
#endif /* HWLOC_LIB */

void mu_init( void ) {
    if ( !mu_initialized ) {
        sys.num_parents = 0;
        sys.parents = NULL;

#if defined HWLOC_LIB
        parse_hwloc();
#elif defined IS_BGQ_MACHINE
        set_bgq_info();
#else
        parse_lscpu();
#endif

        mu_initialized = true;
    }
}

void mu_finalize( void ) {
    free(sys.parents);
    mu_initialized = false;
}

int mu_get_system_size( void ) {
    if ( !mu_initialized ) mu_init();
    return sys.size;
}

void mu_get_system_mask(cpu_set_t *mask) {
    if ( !mu_initialized ) mu_init();
    memcpy(mask, &sys.sys_mask, sizeof(cpu_set_t));
}

// Return Mask of sockets covering at least 1 CPU of cpuset
void mu_get_parents_covering_cpuset(cpu_set_t *parent_set, const cpu_set_t *cpuset) {
    if (!mu_initialized) mu_init();
    CPU_ZERO(parent_set);
    int i;
    for (i=0; i<sys.num_parents; ++i) {
        cpu_set_t intxn;
        CPU_AND(&intxn, &sys.parents[i], cpuset);
        if (CPU_COUNT(&intxn) > 0) {
            CPU_OR(parent_set, parent_set, &sys.parents[i]);
        }
    }
}

// Return Mask of sockets containing all CPUs in cpuset
void mu_get_parents_inside_cpuset(cpu_set_t *parent_set, const cpu_set_t *cpuset) {
    if (!mu_initialized) mu_init();
    CPU_ZERO(parent_set);
    int i;
    for (i=0; i<sys.num_parents; ++i) {
        if (mu_is_subset(&sys.parents[i], cpuset)) {
            CPU_OR(parent_set, parent_set, &sys.parents[i]);
        }
    }
}

/* Returns true is all bits in subset are set in superset */
bool mu_is_subset(const cpu_set_t *subset, const cpu_set_t *superset) {
    // The condition is true if the intersection is identical to subset
    cpu_set_t intxn;
    CPU_AND(&intxn, subset, superset);
    return CPU_EQUAL(&intxn, subset);
}

/* Return the minuend after substracting the bits in substrahend */
void mu_substract(cpu_set_t *result, const cpu_set_t *minuend, const cpu_set_t *substrahend) {
    cpu_set_t xor;
    CPU_XOR(&xor, minuend, substrahend);
    CPU_AND(result, minuend, &xor);
}

// mu_to_str and mu_parse_mask functions are used by DLB utilities
// We export their dynamic symbols to avoid code duplication,
// although they do not belong to the public API
#pragma GCC visibility push(default)
const char* mu_to_str( const cpu_set_t *mask ) {
    if ( !mu_initialized ) mu_init();

    int i;
    static char str[CPU_SETSIZE*4];
    char str_i[16];
    strcpy( str, "[ " );
    for ( i=0; i<sys.size; i++ ) {
        if ( CPU_ISSET(i, mask) ) {
            snprintf(str_i, sizeof(str_i), "%d ", i);
            strcat( str, str_i );
        } else { strcat( str,"- " ); }
    }
    strcat( str, "]\0" );
    return str;
}

void mu_parse_mask( const char *str, cpu_set_t *mask ) {
    if ( !str || strlen(str) == 0 ) return;
    if ( !mu_initialized ) mu_init();

    regex_t regex_bitmask;
    regex_t regex_range;
    CPU_ZERO( mask );

    /* Compile regular expression */
    if ( regcomp(&regex_bitmask, "^[0-1][0-1]+[bB]$", REG_EXTENDED|REG_NOSUB) ) {
        fatal0( "Could not compile regex");
    }

    if ( regcomp(&regex_range, "^[0-9,-]+$", REG_EXTENDED|REG_NOSUB) ) {
        fatal0( "Could not compile regex");
    }

    /* Regular expression matches bitmask, e.g.: 11110011b */
    if ( !regexec(&regex_bitmask, str, 0, NULL, 0) ) {
        // Parse
        int i;
        for (i=0; i<strlen(str); i++) {
            if ( str[i] == '1' && i < sys.size ) {
                CPU_SET( i, mask );
            }
        }
    }
    /* Regular expression matches range, e.g.: 0-3,6-7 */
    else if ( !regexec(&regex_range, str, 0, NULL, 0) ) {
        // Parse
        const char *ptr = str;
        char *endptr;
        while ( ptr < str+strlen(str) ) {
            // Discard junk at the left
            if ( !isdigit(*ptr) ) { ptr++; continue; }

            unsigned int start = strtoul( ptr, &endptr, 10 );
            ptr = endptr;

            // Single element
            if ( (*ptr == ',' || *ptr == '\0') && start < sys.size ) {
                CPU_SET( start, mask );
                ptr++;
                continue;
            }
            // Range
            else if ( *ptr == '-' ) {
                // Discard '-' and possible junk
                ptr++;
                if ( !isdigit(*ptr) ) { ptr++; continue; }

                unsigned int end = strtoul( ptr, &endptr, 10 );
                ptr = endptr;

                // Valid range
                if ( end > start ) {
                    int i;
                    for ( i=start; i<=end && i<sys.size; i++ ) {
                        CPU_SET( i, mask );
                    }
                }
                continue;
            }
            // Unexpected token
            else { }
        }
    }
    /* Regular expression does not match */
    else { }

    regfree(&regex_bitmask);
    regfree(&regex_range);

    if ( CPU_COUNT(mask) == 0 ) {
        warning( "Parsed mask \"%s\" does not seem to be a valid mask\n", str );
    }
}
#pragma GCC visibility pop

void mu_testing_set_sys_size(int size) {
    // For testing purposes only
    sys.size = size;
    int i;
    for (i=0; i<size; ++i) {
        CPU_SET(i, &sys.sys_mask);
    }
}
