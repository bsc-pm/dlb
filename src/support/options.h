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

#ifndef OPTIONS_H
#define OPTIONS_H

#include "support/types.h"

// internal getters
const char* options_get_policy(void);
bool options_get_statistics(void);
bool options_get_drom(void);
bool options_get_just_barier(void);
blocking_mode_t options_get_lend_mode(void);
verbose_opts_t options_get_verbose(void);
verbose_fmt_t options_get_verbose_fmt(void);
bool options_get_trace_enabled(void);
bool options_get_trace_counters(void);
const char* options_get_mask(void);
bool options_get_greedy(void);
const char* options_get_shm_key(void);
bool options_get_bind(void);
const char* options_get_thread_distribution(void);
bool options_get_aggressive_init(void);
bool options_get_priorize_locality(void);
verbose_fmt_t options_get_debug_opts(void);

void options_init(void);
void options_finalize(void);
int options_set_variable(const char *var_name, const char *value);
int options_get_variable(const char *var_name, char *value);
void options_print_variables(void);

#endif /* OPTIONS_H */
