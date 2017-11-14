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

#include "apis/dlb.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <getopt.h>

static void print_usage( const char * program ) {
    fprintf( stdout, "usage: %s [-h] [--help] [--help-extra] [-v] [--version]\n", program );
}

static void print_help(bool print_help, bool print_extra) {
    DLB_Init(0, NULL, NULL);
    if (print_help) DLB_PrintVariables(false);
    if (print_extra) DLB_PrintVariables(true);
    DLB_Finalize();
}

static void print_version( void ) {
    fprintf( stdout, "%s %s (%s)\n", PACKAGE, VERSION, DLB_BUILD_VERSION );
    fprintf( stdout, "Configured with: %s\n", DLB_CONFIGURE_ARGS );
}

int main ( int argc, char *argv[] ) {
    bool do_help = false;
    bool do_help_extra = false;
    bool do_version = false;

    int opt;
    extern char *optarg;
    extern int optind;
    struct option long_options[] = {
        {"help",        no_argument,       0, 'h'},
        {"help-extra",  no_argument,       0, 'x'},
        {"version",     no_argument,       0, 'v'},
        {0,             0,                 0, 0 }
    };

    while ((opt = getopt_long(argc, argv, "hxv", long_options, NULL)) != -1) {
        switch (opt) {
        case 'h':
            do_help = true;
            break;
        case 'x':
            do_help_extra = true;
            break;
        case 'v':
            do_version = true;
            break;
        default:
            print_usage( argv[0] );
            exit(0);
        }
    }

    if (!(do_help || do_help_extra || do_version)) {
        print_usage( argv[0] );
        exit(0);
    }

    /* --help* takes precedence over --version */
    if (do_help || do_help_extra) {
        print_help(do_help, do_help_extra);
        exit(0);
    }

    if (do_version) {
        print_version();
    }

    return 0;
}
