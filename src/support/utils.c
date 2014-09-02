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

#include "utils.h"
#include "debug.h"
#include <stdlib.h>
#include <string.h>

void parse_env_bool ( const char *env, bool *var )
{
   char *tmp_var = getenv( env );

   if ( tmp_var == NULL ) {
      *var = false;
   } else if ( strcasecmp( tmp_var, "YES" ) == 0 ) {
      *var = true;
   } else if ( strcasecmp( tmp_var, "1" ) == 0 ) {
      *var = true;
   } else {
      *var = false;
   }
}

void parse_env_int ( char const *env, int *var )
{
   char *tmp_var = getenv( env );

   if ( tmp_var == NULL || tmp_var[0] == 0 ) {
      *var = 0;
   } else {
      *var = atoi( tmp_var );
   }
}

void parse_env_int_or_die ( char const *env, int *var )
{
   char *tmp_var = getenv( env );

   if ( tmp_var == NULL || tmp_var[0] == 0 ) {
      fatal0( "%s must be defined\n", env );
   } else {
      *var = atoi( tmp_var );
   }
}

void parse_env_string ( char const *env, char **var )
{
   *var = getenv( env );
}

void parse_env_blocking_mode ( char const *env, blocking_mode_t *mode )
{
   char* blocking = getenv( env );

   if ( blocking != NULL ) {
      if ( strcasecmp( blocking, "1CPU" ) == 0 )
         *mode = ONE_CPU;
      else if ( strcasecmp( blocking, "BLOCK" ) == 0 )
         *mode = BLOCK;
   }
}

int my_round ( double x )
{
   int val = x;
   //return (int)((x+x+1.0)/2);
   if ((x - val)>0.5) val++;
   return val;
}
