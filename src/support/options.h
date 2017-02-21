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

#ifndef OPTIONS_H
#define OPTIONS_H

#include "support/types.h"
#include <sched.h>

#define MAX_OPTIONS 32
#define MAX_OPTION_LENGTH 32
#define MAX_DESCRIPTION 1024

typedef struct Options {
    policy_t lb_policy;
    bool statistics;
    bool drom;
    bool barrier;
    bool mpi_just_barrier;
    blocking_mode_t mpi_lend_mode;
    verbose_opts_t verbose;
    verbose_fmt_t verbose_fmt;
    bool trace_enabled;
    bool trace_counters;
    bool greedy;
    char shm_key[MAX_OPTION_LENGTH];
    bool aggressive_init;
    priority_t priority;
    debug_opts_t debug_opts;
    interaction_mode_t mode;
} options_t;

void options_init(options_t *options, const char *lb_args_from_api);
int options_set_variable(options_t *options, const char *var_name, const char *value);
int options_get_variable(const options_t *options, const char *var_name, char *value);
void options_print_variables(const options_t *options);

#endif /* OPTIONS_H */
