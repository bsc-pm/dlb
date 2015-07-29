/*********************************************************************************/
/*  Copyright 2015 Barcelona Supercomputing Center                               */
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

#define _GNU_SOURCE
#include <sched.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <ctype.h>
#include <regex.h>
#include <stdbool.h>
#include "LB_core/DLB_interface.h"

// The next two functions are a copy/paste of the ones defined in the mask_utils
// module. Due to symbol visibility they are replicated here, although a better
// solution that considered better maintainability would be preferred
static const char* mu_to_str ( const cpu_set_t *cpu_set ) {
    int sys_size = DLB_Drom_GetNumCpus();
    int i;
    static char str[CPU_SETSIZE*4];
    char str_i[8];
    strcpy( str, "[ " );
    for ( i=0; i<sys_size; i++ ) {
        if ( CPU_ISSET(i, cpu_set) ) {
            snprintf(str_i, sizeof(str_i), "%d ", i);
            strcat( str, str_i );
        } else { strcat( str,"- " ); }
    }
    strcat( str, "]\0" );
    return str;
}

static void mu_parse_mask( char *str, cpu_set_t *mask ) {
    int sys_size = DLB_Drom_GetNumCpus();
    regex_t regex_bitmask;
    regex_t regex_range;
    CPU_ZERO( mask );

    /* Compile regular expression */
    if ( regcomp(&regex_bitmask, "^[0-1]+$", REG_EXTENDED|REG_NOSUB) ) {
        fprintf(stderr, "Could not compile regex");
    }

    if ( regcomp(&regex_range, "^[0-9,-]+$", REG_EXTENDED|REG_NOSUB) ) {
        fprintf(stderr, "Could not compile regex");
    }

    /* Regular expression matches bitmask, e.g.: 11110011 */
    if ( !regexec(&regex_bitmask, str, 0, NULL, 0) ) {
        // Parse
        int i;
        for (i=0; i<strlen(str); i++) {
            if ( str[i] == '1' && i < sys_size ) {
                CPU_SET( i, mask );
            }
        }
    }
    /* Regular expression matches range, e.g.: 0-3,6-7 */
    else if ( !regexec(&regex_range, str, 0, NULL, 0) ) {
        // Parse
        char *ptr = str;
        char *endptr;
        while ( ptr < str+strlen(str) ) {
            // Discard junk at the left
            if ( !isdigit(*ptr) ) { ptr++; continue; }

            unsigned int start = strtoul( ptr, &endptr, 10 );
            ptr = endptr;

            // Single element
            if ( (*ptr == ',' || *ptr == '\0') && start < sys_size ) {
                CPU_SET( start, mask );
                ptr++;
                continue;
            }
            // Range
            else if ( *ptr == '-' ) {
                ptr++;
                if ( !isdigit(*ptr) ) { ptr++; continue; }
                unsigned int end = strtoul( ptr, &endptr, 10 );
                if ( end > start ) {
                    int i;
                    for ( i=start; i<=end && i<sys_size; i++ ) {
                        CPU_SET( i, mask );
                    }
                }
                ptr++;
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
}

static void print_usage( const char * program ) {
    fprintf( stdout, "usage: %s OPTION [OPTION]...\n", program );
    fprintf( stdout, "Try '%s --help' for more information.\n", program );
}

static void print_help( const char * program ) {
    fprintf( stdout, "DLB - Dynamic Load Balancing, version %s.\n", VERSION );
    fprintf( stdout, "usage: %s PID [CPU-LIST]\n", program );
    fprintf( stdout, "\n" );
    fprintf( stdout, "Show or change the CPU affinity of a DLB process.\n" );
    fprintf( stdout, "\n" );
    fprintf( stdout, "Options:\n" );
    fprintf( stdout, "  -p, --pid          operate on existing given pid\n" );
    fprintf( stdout, "  -c, --cpu-list     change affinity according to list format\n" );
    fprintf( stdout, "  -h, --help         print this help\n" );
}


static void set_affinity( pid_t pid, char *cpu_list ) {
    int error;
    cpu_set_t new_mask;
    mu_parse_mask( cpu_list, &new_mask );

    DLB_Drom_Init();
    error = DLB_Drom_SetProcessMask( pid, &new_mask );
    DLB_Drom_Finalize();

    if ( error ) {
        fprintf( stderr, "Could not find PID %d in DLB shared memory\n", pid );
    } else {
        fprintf( stdout, "PID %d's current affinity set to: %s\n", pid, mu_to_str(&new_mask) );
    }
}

static void show_affinity( pid_t pid ) {
    int error;
    cpu_set_t mask;

    DLB_Drom_Init();
    error = DLB_Drom_GetProcessMask( pid, &mask );
    DLB_Drom_Finalize();

    if ( error ) {
        fprintf( stderr, "Could not find PID %d in DLB shared memory\n", pid );
    } else {
        fprintf( stdout, "PID %d's current affinity CPU list: %s\n", pid, mu_to_str(&mask) );
    }
}


int main ( int argc, char *argv[] ) {
    bool do_help = false;

    pid_t pid = 0;
    char *cpu_list = NULL;

    int opt;
    extern char *optarg;
    struct option long_options[] = {
        {"pid",      required_argument, 0, 'p'},
        {"cpu-list", required_argument, 0, 'c'},
        {"help",     no_argument,       0, 'h'},
        {0,          0,                 0, 0 }
    };

    while ( (opt = getopt_long(argc, argv, "p:c:h", long_options, NULL)) != -1 ) {
        switch (opt) {
        case 'h':
            do_help = true;
            break;
        case 'p':
            pid = strtoul( optarg, NULL, 10 );
            break;
        case 'c':
            cpu_list = (char*) malloc( strlen(optarg) );
            strcpy( cpu_list, optarg );
            break;
        default:
            print_usage( argv[0] );
            exit( EXIT_SUCCESS );
        }
    }

    if ( !do_help && !pid ) {
        print_usage( argv[0] );
        exit( EXIT_SUCCESS );
    }

    if ( do_help ) {
        print_help(argv[0] );
        exit( EXIT_SUCCESS );
    }

    if ( cpu_list ) {
        set_affinity( pid, cpu_list );
    } else {
        show_affinity( pid );
    }

    free( cpu_list );
    return EXIT_SUCCESS;;
}
