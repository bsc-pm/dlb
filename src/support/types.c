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

#include <stdlib.h>
#include <string.h>
#include "support/types.h"

void parse_bool(const char *str, bool *value) {
    if (strcasecmp(str, "1")==0        ||
            strcasecmp(str, "yes")==0  ||
            strcasecmp(str, "true")==0) {
        *value = true;
    } else if (strcasecmp(str, "0")==0 ||
            strcasecmp(str, "no")==0   ||
            strcasecmp(str, "false")==0) {
        *value = false;
    }
}

void parse_int(const char *str, int *value) {
    *value = atoi(str);
}

void parse_blocking_mode(const char *str, blocking_mode_t *value) {
    if (strcasecmp(str, "1CPU") == 0) {
        *value = ONE_CPU;
    } else if (strcasecmp(str, "BLOCK") == 0) {
        *value = BLOCK;
    }
}

void parse_verbose_opts(const char *str, verbose_opts_t *value) {
    *value = VB_CLEAR;
    char *my_str = (char*)malloc(strlen(str)+1);
    strncpy(my_str, str, strlen(str)+1);
    char *saveptr;
    const char delimiter[2] = ":";
    char *token = strtok_r(my_str, delimiter, &saveptr);
    while (token != NULL) {
        if ( !(*value & VB_API) && !strcasecmp(token, "api") ) {
            *value |= VB_API;
        } else if ( !(*value & VB_MICROLB) && !strcasecmp(token, "microlb") ) {
            *value |= VB_MICROLB;
        } else if ( !(*value & VB_SHMEM) && !strcasecmp(token, "shmem") ) {
            *value |= VB_SHMEM;
        } else if ( !(*value & VB_MPI_API) && !strcasecmp(token, "mpi_api") ) {
            *value |= VB_MPI_API;
        } else if ( !(*value & VB_MPI_INT) && !strcasecmp(token, "mpi_intercept") ) {
            *value |= VB_MPI_INT;
        } else if ( !(*value & VB_STATS) && !strcasecmp(token, "stats") ) {
            *value |= VB_STATS;
        } else if ( !(*value & VB_DROM) && !strcasecmp(token, "drom") ) {
            *value |= VB_DROM;
        }
        token = strtok_r(NULL, delimiter, &saveptr);
    }
    free(my_str);
}

void parse_verbose_fmt(const char *str, verbose_fmt_t *value) {
    *value = VBF_CLEAR;
    char *my_str = (char*)malloc(strlen(str)+1);
    strncpy(my_str, str, strlen(str)+1);
    char *saveptr;
    const char delimiter[2] = ":";
    char *token = strtok_r(my_str, delimiter, &saveptr);
    while (token != NULL) {
        if ( !(*value & VBF_NODE) && !strcasecmp(token, "node") ) {
            *value |= VBF_NODE;
        } else if ( !(*value & VBF_PID) && !strcasecmp(token, "pid") ) {
            *value |= VBF_PID;
        } else if ( !(*value & VBF_MPINODE) && !strcasecmp(token, "mpinode") ) {
            *value |= VBF_MPINODE;
        } else if ( !(*value & VBF_MPIRANK) && !strcasecmp(token, "mpirank") ) {
            *value |= VBF_MPIRANK;
        } else if ( !(*value & VBF_THREAD) && !strcasecmp(token, "thread") ) {
            *value |= VBF_THREAD;
        }
        token = strtok_r(NULL, delimiter, &saveptr);
    }
    free(my_str);
}

void parse_debug_opts(const char *str, debug_opts_t *value) {
    *value = DBG_CLEAR;
    char *my_str = (char*)malloc(strlen(str)+1);
    strncpy(my_str, str, strlen(str)+1);
    char *saveptr;
    const char delimiter[2] = ":";
    char *token = strtok_r(my_str, delimiter, &saveptr);
    while (token != NULL) {
        if ( !(*value & DBG_NOREGSIGNAL) && !strcasecmp(token, "no-register-signals") ) {
            *value |= DBG_NOREGSIGNAL;
        } else if ( !(*value & DBG_RETURNSTOLEN) && !strcasecmp(token, "return-stolen") ) {
            *value |= DBG_RETURNSTOLEN;
        }
        token = strtok_r(NULL, delimiter, &saveptr);
    }
    free(my_str);
}
