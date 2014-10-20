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

#ifndef UTILS_H
#define UTILS_H

#define _GNU_SOURCE
#include <sched.h>
#include <stdbool.h>


typedef enum {
   ONE_CPU, // MPI not set to blocking, leave a cpu while in a MPI blockin call
   BLOCK,   // MPI set to blocking mode
} blocking_mode_t;


void parse_env_bool ( const char *env, bool *var );
void parse_env_int ( char const *env, int *var );
void parse_env_int_or_die ( char const *env, int *var );
void parse_env_string ( char const *env, char **var );
void parse_env_string_or_die ( char const *env, char **var );
void parse_env_blocking_mode ( char const *env, blocking_mode_t *mode );

int my_round ( double x );

#endif /* UTILS_H */
