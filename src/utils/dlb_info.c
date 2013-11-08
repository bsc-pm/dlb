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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "support/utils.h"

void print_usage( const char * program )
{
   fprintf( stdout, "usage: %s [-h] [--help] [-v] [--version]\n", program );
}

void print_help( void )
{
   fprintf( stdout, "DLB - Dynamic Load Balancing, version %s.\n", VERSION );
   fprintf( stdout, "\n" );
   fprintf( stdout, "DLB Environment Variables:\n" );
   fprintf( stdout, "LB_JUST_BARRIER = [0, 1]\n" );
   fprintf( stdout, "LB_LEND_MODE = [BLOCK, 1CPU]\n" );
   fprintf( stdout, "LB_AGGRESSIVE_INIT = [0, 1]\n" );
   fprintf( stdout, "LB_PRIORIZE_LOCALITY = [0, 1]\n" );
   fprintf( stdout, "LB_POLICY = [policy]\n" );
   fprintf( stdout, "\n" );
   fprintf( stdout, "LB Policies:\n" );
   fprintf( stdout, "- No: \n" );
   fprintf( stdout, "- LeWI:\n" );
   fprintf( stdout, "- LeWI_mask:\n" );
   fprintf( stdout, "- RaL:\n" );
}

void print_version( void )
{
   fprintf( stdout, "%s %s (%s)\n", PACKAGE, VERSION, DLB_BUILD_VERSION );
   fprintf( stdout, "Configured with: %s\n", DLB_CONFIGURE_ARGS );
}

int main ( int argc, char *argv[] )
{
   bool list_help = false;
   bool list_version = false;

   int i;
   for ( i=1; i<argc; i++ ) {
      if ( strcmp( argv[i], "--help" ) == 0 ||
           strcmp( argv[i], "-h" ) == 0 ) {
         list_help = true;
      } else if ( strcmp( argv[i], "--version" ) == 0 ||
                  strcmp( argv[i], "-v" ) == 0 ) {
         list_version = true;
      } else {
         print_usage( argv[0] );
         exit(0);
      }
   }

   if ( !list_help && !list_version ) {
      print_usage( argv[0] );
      exit(0);
   }

   if ( list_help )
      print_help();

   if ( list_version )
      print_version();

   return 0;
}
