/*********************************************************************************/
/*  Copyright 2009-2024 Barcelona Supercomputing Center                          */
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

#ifndef OPTIONS_H
#define OPTIONS_H

#include "support/types.h"
#include <sched.h>
#include <sys/types.h>

enum { MAX_OPTION_LENGTH = 64 };
enum { MAX_DESCRIPTION = 1024 };

typedef struct Options {
    /* general options */
    bool                lewi;
    bool                drom;
    bool                barrier;
    bool                ompt;
    interaction_mode_t  mode;
    /* verbose */
    bool                quiet;
    bool                silent;
    verbose_opts_t      verbose;
    verbose_fmt_t       verbose_fmt;
    /* instrument */
    instrument_items_t  instrument;
    bool                instrument_counters;
    int                 instrument_extrae_nthreads;
    /* lewi */
    bool                lewi_keep_cpu_on_blocking_call;
    bool                lewi_respect_cpuset;
    bool                lewi_greedy;
    bool                lewi_warmup;
    mpi_set_t           lewi_mpi_calls;
    bool                lewi_barrier;
    char                lewi_barrier_select[MAX_OPTION_LENGTH];
    priority_t          lewi_affinity;
    omptool_opts_t      lewi_ompt;
    int                 lewi_max_parallelism;
    int                 lewi_color;
    /* misc */
    char                shm_key[MAX_OPTION_LENGTH];
    pid_t               preinit_pid;
    debug_opts_t        debug_opts;
    omptm_version_t     omptm_version;
    /* talp */
    bool                talp;
    bool                talp_openmp;
    bool                talp_papi;
    bool                talp_external_profiler;
    talp_summary_t      talp_summary;
    char                *talp_output_file;
    int                 talp_regions_per_proc;
    /* barrier */
    int                 barrier_id;
} options_t;

void options_init(options_t *options, const char *dlb_args);
void options_finalize(options_t *options);
void options_parse_entry(const char *var_name, void *option);
int options_set_variable(options_t *options, const char *var_name, const char *value);
int options_get_variable(const options_t *options, const char *var_name, char *value);
void options_print_variables(const options_t *options, bool print_extended);
void options_print_lewi_flags(const options_t *options);

#endif /* OPTIONS_H */
