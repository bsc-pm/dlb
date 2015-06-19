/*************************************************************************************/
/*      Copyright 2014 Barcelona Supercomputing Center                               */
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

void parse_env_bool ( const char *env, bool *var, bool default_value ) {
    *var = default_value;
    char *tmp_var = getenv( env );
    if ( tmp_var ) {
        if ( strcasecmp(tmp_var, "1")==0        ||
                strcasecmp(tmp_var, "yes")==0   ||
                strcasecmp(tmp_var, "true")==0 ) {
            *var = true;
        } else if ( strcasecmp(tmp_var, "0")==0 ||
                strcasecmp(tmp_var, "no")==0    ||
                strcasecmp(tmp_var, "false")==0 ) {
            *var = false;
        }
    }
}

void parse_env_int ( char const *env, int *var ) {
    char *tmp_var = getenv( env );

    if ( tmp_var == NULL || tmp_var[0] == 0 ) {
        *var = 0;
    } else {
        *var = atoi( tmp_var );
    }
}

void parse_env_int_or_die ( char const *env, int *var ) {
    char *tmp_var = getenv( env );

    if ( tmp_var == NULL || tmp_var[0] == 0 ) {
        fatal0( "%s must be defined\n", env );
    } else {
        *var = atoi( tmp_var );
    }
}

void parse_env_string ( char const *env, char **var ) {
    *var = getenv( env );
}

void parse_env_string_or_die ( char const *env, char **var ) {
    *var = getenv( env );
    if ( *var == NULL ) {
        fatal0( "%s must be defined\n", env );
    }
}

void parse_env_blocking_mode ( char const *env, blocking_mode_t *mode ) {
    char *blocking = getenv( env );

    if ( blocking != NULL ) {
        if ( strcasecmp( blocking, "1CPU" ) == 0 ) {
            *mode = ONE_CPU;
        } else if ( strcasecmp( blocking, "BLOCK" ) == 0 ) {
            *mode = BLOCK;
        }
    }
}

void parse_env_verbose_opts ( char const *env, verbose_opts_t *mode ) {
    *mode = VB_CLEAR;
    char *str_mode = getenv( env );
    if ( str_mode != NULL ) {
        const char delimiter[2] = ":";
        char *token = strtok( str_mode, delimiter );
        while( token != NULL ) {
            if ( !(*mode & VB_API) && !strcasecmp( token, "api" ) ) {
                *mode |= VB_API;
            } else if ( !(*mode & VB_MICROLB) && !strcasecmp( token, "microlb" ) ) {
                *mode |= VB_MICROLB;
            } else if ( !(*mode & VB_SHMEM) && !strcasecmp( token, "shmem" ) ) {
                *mode |= VB_SHMEM;
            } else if ( !(*mode & VB_MPI_API) && !strcasecmp( token, "mpi_api" ) ) {
                *mode |= VB_MPI_API;
            } else if ( !(*mode & VB_MPI_INT) && !strcasecmp( token, "mpi_intercept" ) ) {
                *mode |= VB_MPI_INT;
            } else if ( !(*mode & VB_STATS) && !strcasecmp( token, "stats" ) ) {
                *mode |= VB_STATS;
            } else if ( !(*mode & VB_DROM) && !strcasecmp( token, "drom" ) ) {
                *mode |= VB_DROM;
            }
            token = strtok( NULL, delimiter );
        }
    }
}

void parse_env_verbose_format ( char const *env, verbose_fmt_t *format,
                                        verbose_fmt_t default_format ) {
    *format = VBF_CLEAR;
    char *str_format = getenv( env );
    if ( str_format != NULL ) {
        const char delimiter[2] = ":";
        char *token = strtok( str_format, delimiter );
        while( token != NULL ) {
            if ( !(*format & VBF_NODE) && !strcasecmp( token, "node" ) ) {
                *format |= VBF_NODE;
            } else if ( !(*format & VBF_PID) && !strcasecmp( token, "pid" ) ) {
                *format |= VBF_PID;
            } else if ( !(*format & VBF_MPINODE) && !strcasecmp( token, "mpinode" ) ) {
                *format |= VBF_MPINODE;
            } else if ( !(*format & VBF_MPIRANK) && !strcasecmp( token, "mpirank" ) ) {
                *format |= VBF_MPIRANK;
            } else if ( !(*format & VBF_THREAD) && !strcasecmp( token, "thread" ) ) {
                *format |= VBF_THREAD;
            }
            token = strtok( NULL, delimiter );
        }
    }
    if ( *format == VBF_CLEAR ) {
        *format = default_format;
    }
}

int my_round ( double x ) {
    int val = x;
    //return (int)((x+x+1.0)/2);
    if ((x - val)>0.5) { val++; }
    return val;
}
