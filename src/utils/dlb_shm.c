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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include "support/utils.h"
#include "LB_core/DLB_interface.h"

static char * created_shm_filename = NULL;
static char * userdef_shm_filename = NULL;

void print_usage( const char * program )
{
   fprintf( stdout, "usage: %s [-h] [--help] [-c] [--create] [-l] [--list] [-d] [--delete]\n", program );
}

void print_help( void )
{
   fprintf( stdout, "DLB - Dynamic Load Balancing, version %s.\n", VERSION );
   fprintf( stdout, "\n" );
   fprintf( stdout, "COMMANDS:\n" );
   fprintf( stdout, " - create[-c]: Create and empty shmem data\n" );
   fprintf( stdout, " - list[-l]:   Print DLB shmem data, if any\n" );
   fprintf( stdout, " - delete[-d]: Delete shmem data\n" );
}

// FIXME
char *get_shm_filename( void );
void create_shdata( void )
{
   char *policy = getenv( "LB_POLICY" );
   if ( policy == NULL ) {
      fprintf( stdout, "Creating shmem without policy... Setting auto_LeWI_mask as default.\n" );
      setenv( "LB_POLICY", "auto_LeWI_mask", 1 );
   }

   DLB_Init();
   created_shm_filename = get_shm_filename();
   fprintf( stdout, "Succesfully created Shared Memory: %s\n", created_shm_filename );
   DLB_Finalize();
}

// FIXME
void get_masks( cpu_set_t *given,  cpu_set_t *avail,  cpu_set_t *not_borrowed, cpu_set_t *check );
const char* mu_to_str ( const cpu_set_t *cpu_set );
void mu_init( void );
void mu_finalize( void );

void list_shdata_item( const char* name )
{
   setenv( "LB_SHM_NAME", name, 1);
   cpu_set_t given, avail, not_borrowed, check;
   get_masks( &given, &avail, &not_borrowed, &check );
   fprintf( stderr, "Given CPUs:        %s\n", mu_to_str( &given ) );
   fprintf( stderr, "Available CPUs:    %s\n", mu_to_str( &avail ) );
   fprintf( stderr, "Not borrowed CPUs: %s\n", mu_to_str( &not_borrowed ) );
   fprintf( stderr, "Running CPUs:      %s\n", mu_to_str( &check ) );
}

void list_shdata( void )
{
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

   mu_init();
   chdir(dir);
   while ( (entry = readdir(dp)) != NULL ) {
      printf ("%s\n", entry->d_name);
      lstat( entry->d_name, &statbuf );
      if( S_ISREG( statbuf.st_mode ) && getuid() == statbuf.st_uid ) {
         if ( fnmatch( "DLB_shm_*", entry->d_name, 0) == 0 ) {
            fprintf( stdout, "Found DLB shmem: %s\n", entry->d_name );
            list_shdata_item( &(entry->d_name[8]) );

            // Double-check if recently created or user defined
            // Shared Memories are detected. (BG/Q needs it)
            created_shm_listed = created_shm_listed || strstr( created_shm_filename, entry->d_name );
            userdef_shm_listed = userdef_shm_listed || strstr( userdef_shm_filename, entry->d_name );
         }
      }
   }

   if ( !created_shm_listed ) {
      fprintf( stderr, "Previously created Shared Memory file not found.\n" );
      fprintf( stderr, "Looking for %s...\n", created_shm_filename );
      list_shdata_item( &(created_shm_filename[9]) );
   }

   if ( !userdef_shm_listed ) {
      fprintf( stderr, "User defined Shared Memory file not found.\n" );
      fprintf( stderr, "Looking for %s...\n", userdef_shm_filename );
      list_shdata_item( &(userdef_shm_filename[8]) );
   }
   closedir(dp);
   mu_finalize();
}

void delete_shdata_item( const char* name )
{
   fprintf( stdout, "Deleting... %s\n", name );
   if ( unlink( name ) ) {
      perror( "DLB ERROR: can't delete shm file" );
      exit( EXIT_FAILURE );
   }
}

void delete_shdata( void )
{
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
         if ( fnmatch( "DLB_shm_*", entry->d_name, 0) == 0    ||
              fnmatch( "sem.DLB_sem_*", entry->d_name, 0) == 0 ) {
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

int main ( int argc, char *argv[] )
{
   bool do_help = false;
   bool do_create = false;
   bool do_list = false;
   bool do_delete = false;

   int i;
   for ( i=1; i<argc; i++ ) {
      if ( strcmp( argv[i], "--help" ) == 0 ||
           strcmp( argv[i], "-h" ) == 0 ) {
         do_help = true;
      } else if ( strcmp( argv[i], "--create" ) == 0 ||
                  strcmp( argv[i], "-c" ) == 0 ) {
         do_create = true;
      } else if ( strcmp( argv[i], "--list" ) == 0 ||
                  strcmp( argv[i], "-l" ) == 0 ) {
         do_list = true;
      } else if ( strcmp( argv[i], "--delete" ) == 0 ||
                  strcmp( argv[i], "-d" ) == 0 ) {
         do_delete = true;
      } else {
         print_usage( argv[0] );
         exit(0);
      }
   }

   if ( !do_help && !do_create && !do_list && !do_delete ) {
      print_usage( argv[0] );
      exit( EXIT_SUCCESS );
   }

   if ( do_help )
      print_help();

   if ( do_create )
      create_shdata();

   if ( do_list )
      list_shdata();

   if ( do_delete )
      delete_shdata();

   return 0;
}
