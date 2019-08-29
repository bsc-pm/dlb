/*********************************************************************************/
/*  Copyright 2009-2019 Barcelona Supercomputing Center                          */
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

/*! \page dlb_shm Manage DLB shared memory.
 *  \section synopsis SYNOPSIS
 *      <B>dlb_shm</B> {--list | --delete | --help}
 *  \section description DESCRIPTION
 *      Utility command to list or delete the DLB shared memory.
 *
 *      <DL>
 *          <DT>-l, --list</DT>
 *          <DD>Print the DLB shared memory data.</DD>
 *
 *          <DT>-d, --delete</DT>
 *          <DD>Delete the DLB shared memory.</DD>
 *
 *          <DT>-h, --help</DT>
 *          <DD>Print usage.</DD>
 *      </DL>
 *  \section author AUTHOR
 *      Barcelona Supercomputing Center (pm-tools@bsc.es)
 *  \section seealso SEE ALSO
 *      \ref dlb "dlb"(1), \ref dlb_run "dlb_run"(1),
 *      \ref dlb_taskset "dlb_taskset"(1)
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "apis/dlb.h"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fnmatch.h>
#include <dirent.h>
#include <sys/stat.h>
#include <getopt.h>
#include <stdbool.h>
#include <limits.h>

static char *created_shm_filename = NULL;
static char *userdef_shm_filename = NULL;

static void __attribute__((__noreturn__)) version(void) {
    fprintf(stdout, "%s %s (%s)\n", PACKAGE, VERSION, DLB_BUILD_VERSION);
    fprintf(stdout, "Configured with: %s\n", DLB_CONFIGURE_ARGS);
    exit(EXIT_SUCCESS);
}

static void __attribute__((__noreturn__)) usage(const char *program, FILE *out) {
    fprintf(out, "DLB - Dynamic Load Balancing, version %s.\n", VERSION);
    fprintf(out, (
                "usage:\n"
                "\t%1$s --list\n"
                "\t%1$s --delete\n"
                "\n"
                ), program);

    fputs("Manage DLB shared memory.\n\n", out);

    fputs((
                "Options:\n"
                "  -l, --list               print DLB shmem data, if any\n"
                "  -d, --delete             delete shmem data\n"
                /* Options --create and --file are experimental */
                /* "  -c, --create             create and empty Shared Memory file\n" */
                /* "  -f, --file=FILE          use only this specific Shared Memory file\n" */
                "  -h, --help               print this help\n"
                ), out);

    exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

void create_shdata( void ) {
    fprintf( stdout, "Create shdata: Function currently disabled\n" );
#if 0
    DLB_Init();
    created_shm_filename = get_shm_filename();
    fprintf( stdout, "Succesfully created Shared Memory: %s\n", created_shm_filename );
    DLB_Finalize();
#endif
}

void list_shdata_item(const char* shm_suffix, int list_columns,
        dlb_printshmem_flags_t print_flags) {
    char *p;
    if ( (p = strstr(shm_suffix, "cpuinfo")) ) {
        /* Get pointer to shm_key */
        p = p + 8;  // remove "cpuinfo_"
        fprintf( stdout, "Found DLB shmem with id: %s\n", p );
        /* Modify DLB_ARGS */
        const char *dlb_args_env = getenv("DLB_ARGS");
        size_t dlb_args_env_len = dlb_args_env ? strlen(dlb_args_env) + 1 : 0;
        const char * const new_dlb_args_base = "--verbose-format=node --shm-key=";
        char *dlb_args = malloc(dlb_args_env_len + strlen(new_dlb_args_base) + strlen(p) + 1);
        sprintf(dlb_args, "%s %s%s", dlb_args_env ? dlb_args_env : "", new_dlb_args_base, p);
        setenv("DLB_ARGS", dlb_args, 1);
        free(dlb_args);
        /* Print */
        DLB_PrintShmem(list_columns, print_flags);
    } else if ( (p = strstr(shm_suffix, "procinfo")) ) {
        // We currently print both shmems with the same API. Skipping.
    }
}

void list_shdata(int list_columns, dlb_printshmem_flags_t print_flags) {
    // Ignore them if the filename is not specified
    bool created_shm_listed = created_shm_filename == NULL;
    bool userdef_shm_listed = userdef_shm_filename == NULL;

    const char dir[] = "/dev/shm";
    DIR *dp;
    struct dirent *entry;
    struct stat statbuf;

    if ( (dp = opendir(dir)) == NULL ) {
        perror( "DLB ERROR: can't open /dev/shm" );
        exit( EXIT_FAILURE );
    }

    while ( (entry = readdir(dp)) != NULL ) {
        /* Obtain absolute path to filename */
        int filename_size = snprintf(NULL, 0, "%s/%s", dir, entry->d_name);
        char *filename = malloc((filename_size+1)*sizeof(char));
        sprintf(filename, "%s/%s", dir, entry->d_name);
        /* Only list if filename is same UID and matches DLB_* */
        stat( filename, &statbuf );
        if( getuid() == statbuf.st_uid &&
                fnmatch( "DLB_*", entry->d_name, 0) == 0 ) {
            // Ignore DLB_ prefix
            list_shdata_item(&entry->d_name[4], list_columns, print_flags);

            // Double-check if recently created or user defined
            // Shared Memories are detected. (BG/Q needs it)
            created_shm_listed = created_shm_listed || strstr( created_shm_filename, entry->d_name );
            userdef_shm_listed = userdef_shm_listed || strstr( userdef_shm_filename, entry->d_name );
        }
        free(filename);
    }

    if ( !created_shm_listed ) {
        fprintf( stderr, "Previously created Shared Memory file not found.\n" );
        fprintf( stderr, "Looking for %s...\n", created_shm_filename );
        list_shdata_item(&created_shm_filename[9], list_columns, print_flags);
    }

    if ( !userdef_shm_listed ) {
        fprintf( stderr, "User defined Shared Memory file not found.\n" );
        fprintf( stderr, "Looking for %s...\n", userdef_shm_filename );
        list_shdata_item(&userdef_shm_filename[9], list_columns, print_flags);
    }
}

void delete_shdata_item( const char* name ) {
    fprintf( stdout, "Deleting... %s\n", name );

    if ( unlink( name ) ) {
        perror( "DLB ERROR: can't delete shm file" );
        exit( EXIT_FAILURE );
    }
}

void delete_shdata( void ) {
    // Ignore them if the filename is not specified
    bool created_shm_deleted = created_shm_filename == NULL;
    bool userdef_shm_deleted = userdef_shm_filename == NULL;

    const char dir[] = "/dev/shm";
    DIR *dp;
    struct dirent *entry;
    struct stat statbuf;

    if ( (dp = opendir(dir)) == NULL ) {
        perror( "DLB ERROR: can't open /dev/shm" );
        exit( EXIT_FAILURE );
    }

    chdir(dir);
    while ( (entry = readdir(dp)) != NULL ) {
        lstat( entry->d_name, &statbuf );
        if( S_ISREG( statbuf.st_mode ) && getuid() == statbuf.st_uid ) {
            if ( fnmatch( "DLB_*", entry->d_name, 0) == 0    ||
                    fnmatch( "sem.DLB_*", entry->d_name, 0) == 0 ) {
                delete_shdata_item( entry->d_name );

                // Double-check if recently created or user defined
                // Shared Memories are detected. (BG/Q needs it)
                created_shm_deleted = created_shm_deleted || strstr( created_shm_filename, entry->d_name );
                userdef_shm_deleted = userdef_shm_deleted || strstr( userdef_shm_filename, entry->d_name );
            }
        }
    }

    if ( !created_shm_deleted ) {
        fprintf( stderr, "Previously created Shared Memory file not found.\n" );
        fprintf( stderr, "Trying to delete %s...\n", created_shm_filename );
        delete_shdata_item( created_shm_filename );
    }

    if ( !userdef_shm_deleted ) {
        fprintf( stderr, "User defined Shared Memory file not found.\n" );
        fprintf( stderr, "Trying to delete %s...\n", userdef_shm_filename );
        delete_shdata_item( userdef_shm_filename );
    }

    closedir(dp);
}

int main(int argc, char *argv[]) {
    bool do_create = false;
    bool do_list = false;
    bool do_delete = false;
    int list_columns = 0;
    dlb_printshmem_flags_t print_flags = DLB_COLOR_AUTO;

    /* Long options that have no corresponding short option */
    enum {
        COLOR_OPTION = CHAR_MAX + 1
    };

    int opt;
    struct option long_options[] = {
        {"create",   no_argument,       NULL, 'c'},
        {"list",     optional_argument, NULL, 'l'},
        {"delete",   no_argument,       NULL, 'd'},
        {"file",     required_argument, NULL, 'f'},
        {"color",    optional_argument, NULL, COLOR_OPTION},
        {"help",     no_argument,       NULL, 'h'},
        {"version",  no_argument,       NULL, 'v'},
        {0,          0,                 NULL, 0 }
    };

    while ( (opt = getopt_long(argc, argv, "cl::df:hv", long_options, NULL)) != -1 ) {
        switch (opt) {
            case 'c':
                do_create = true;
                break;
            case 'l':
                do_list = true;
                if (optarg) {
                    list_columns = strtol(optarg, NULL, 0);
                }
                break;
            case 'd':
                do_delete = true;
                break;
            case 'f':
                // Make enough space for '/' and '\0' characters
                userdef_shm_filename = malloc( strlen(optarg) + 2 );
                // Prepend '/' if needed
                sprintf( userdef_shm_filename, "%s%s", optarg[0] == '/' ? "" : "/", optarg );
                break;
            case COLOR_OPTION:
                if (optarg && strcasecmp (optarg, "no") == 0) {
                    print_flags &= ~DLB_COLOR_AUTO;
                    print_flags &= ~DLB_COLOR_ALWAYS;
                } else {
                    print_flags |= DLB_COLOR_ALWAYS;
                }
                break;
            case 'h':
                usage(argv[0], stdout);
                break;
            case 'v':
                version();
                break;
            default:
                usage(argv[0], stderr);
        }
    }

    // Incompatible options
    if (do_create + do_list + do_delete != 1) {
        usage(argv[0], stderr);
    }

    if (do_create) {
        create_shdata();
    }
    else if (do_list) {
        list_shdata(list_columns, print_flags);
    }
    else if (do_delete) {
        delete_shdata();
    }
    else {
        usage(argv[0], stderr);
    }

    free(userdef_shm_filename);
    return EXIT_SUCCESS;
}
