/*********************************************************************************/
/*  Copyright 2009-2021 Barcelona Supercomputing Center                          */
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

#ifndef TYPES_H
#define TYPES_H

#include <stdbool.h>

typedef enum VerboseOptions {
    VB_CLEAR    = 0,
    VB_API      = 1 << 0,
    VB_MICROLB  = 1 << 1,
    VB_SHMEM    = 1 << 2,
    VB_MPI_API  = 1 << 3,
    VB_MPI_INT  = 1 << 4,
    VB_STATS    = 1 << 5,
    VB_DROM     = 1 << 6,
    VB_ASYNC    = 1 << 7,
    VB_OMPT     = 1 << 8,
    VB_AFFINITY = 1 << 9,
    VB_BARRIER  = 1 << 10,
    VB_TALP     = 1 << 11
} verbose_opts_t;

typedef enum VerboseFormat {
    VBF_CLEAR   = 0,
    VBF_NODE    = 1 << 0,
    VBF_MPINODE = 1 << 1,
    VBF_MPIRANK = 1 << 2,
    VBF_SPID    = 1 << 3,
    VBF_THREAD  = 1 << 4,
    VBF_TSTAMP  = 1 << 5
} verbose_fmt_t;

typedef enum InstrumentEvents {
    INST_NONE   = 0,
    INST_ALL    = 0xFFFF,
    INST_MPI    = 1 << 0,
    INST_LEWI   = 1 << 1,
    INST_DROM   = 1 << 2,
    INST_TALP   = 1 << 3,
    INST_BARR   = 1 << 4,
    INST_OMPT   = 1 << 5,
    INST_CPUS   = 1 << 6,
    INST_CBCK   = 1 << 7
} instrument_events_t;

typedef enum DebugOptions {
    DBG_CLEAR        = 0,
    DBG_RETURNSTOLEN = 1 << 0,
    DBG_WERROR       = 1 << 1,
    DBG_LPOSTMORTEM  = 1 << 2
} debug_opts_t;

typedef enum PriorityType {
    PRIO_ANY,
    PRIO_NEARBY_FIRST,
    PRIO_NEARBY_ONLY,
    PRIO_SPREAD_IFEMPTY
} priority_t;

typedef enum TalpSummaryType {
    SUMMARY_NONE        = 0,
    SUMMARY_POP_METRICS = 1 << 1,
    SUMMARY_NODE        = 1 << 2,
    SUMMARY_PROCESS     = 1 << 3,
    SUMMARY_ITERATION   = 1 << 4,
    SUMMARY_OMP         = 1 << 5,
    SUMMARY_REGIONS     = 1 << 6
} talp_summary_t;

typedef enum PolicyType {
    POLICY_NONE,
    POLICY_LEWI,
    POLICY_LEWI_MASK
} policy_t;

typedef enum InteractionMode {
    MODE_POLLING,
    MODE_ASYNC
} interaction_mode_t;

typedef enum MPISet {
    MPISET_ALL,
    MPISET_BARRIER,
    MPISET_COLLECTIVES
} mpi_set_t;

typedef enum OMPTOptions {
    OMPT_OPTS_CLEAR     = 0,
    OMPT_OPTS_MPI       = 1 << 0,
    OMPT_OPTS_BORROW    = 1 << 1,
    OMPT_OPTS_LEND      = 1 << 2
} ompt_opts_t;

static inline int min_int(int a, int b) { return a < b ? a : b; }
static inline int max_int(int a, int b) { return a > b ? a : b; }

int parse_bool(const char *str, bool *value);
int parse_int(const char *str, int *value);

/* verbose_opts_t */
int parse_verbose_opts(const char *str, verbose_opts_t *value);
const char* verbose_opts_tostr(verbose_opts_t value);
const char* get_verbose_opts_choices(void);

/* verbose_fmt_t */
int parse_verbose_fmt(const char *str, verbose_fmt_t *value);
const char* verbose_fmt_tostr(verbose_fmt_t value);
const char* get_verbose_fmt_choices(void);

/* instrument_events_t */
int parse_instrument_events(const char *str, instrument_events_t *value);
const char* instrument_events_tostr(instrument_events_t value);
const char* get_instrument_events_choices(void);

/* debug_opts_t */
int parse_debug_opts(const char *str, debug_opts_t *value);
const char* debug_opts_tostr(debug_opts_t value);
const char* get_debug_opts_choices(void);

/* priority_t */
int parse_priority(const char *str, priority_t *value);
const char* priority_tostr(priority_t value);
const char* get_priority_choices(void);

/* policy_t */
int parse_policy(const char *str, policy_t *value);
const char* policy_tostr(policy_t policy);
const char* get_policy_choices(void);

/* talp_summary_t */
int parse_talp_summary(const char *str, talp_summary_t *value);
const char* talp_summary_tostr(talp_summary_t summary);
const char* get_talp_summary_choices(void);

/* interaction_mode_t */
int parse_mode(const char *str, interaction_mode_t *value);
const char* mode_tostr(interaction_mode_t value);
const char* get_mode_choices(void);

/* mpi_set_t */
int parse_mpiset(const char *str, mpi_set_t *value);
const char* mpiset_tostr(mpi_set_t value);
const char* get_mpiset_choices(void);

/* ompt_opts_t */
int parse_ompt_opts(const char *str, ompt_opts_t *value);
const char* ompt_opts_tostr(ompt_opts_t value);
const char* get_ompt_opts_choices(void);

#endif /* TYPES_H */
