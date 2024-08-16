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
#include <stdint.h>

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
    VB_TALP     = 1 << 11,
    VB_INSTR    = 1 << 12,
    VB_ALL      = 0xFFFF,
    VB_UNDEF    = 0xF0000,
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

typedef enum InstrumentItems {
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
} instrument_items_t;

typedef enum DebugOptions {
    DBG_CLEAR        = 0,
    DBG_RETURNSTOLEN = 1 << 0,
    DBG_WERROR       = 1 << 1,
    DBG_LPOSTMORTEM  = 1 << 2,
    DBG_WARNMPI      = 1 << 3,
} debug_opts_t;

typedef enum PriorityType {
    PRIO_ANY,
    PRIO_NEARBY_FIRST,
    PRIO_NEARBY_ONLY,
    PRIO_SPREAD_IFEMPTY
} priority_t;

typedef enum TalpSummaryType {
    SUMMARY_NONE        = 0,
    SUMMARY_ALL         = 0xFFFF,
    SUMMARY_POP_METRICS = 1 << 0,
    SUMMARY_POP_RAW     = 1 << 1,
    SUMMARY_NODE        = 1 << 2,
    SUMMARY_PROCESS     = 1 << 3,
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
    MPISET_NONE,
    MPISET_ALL,
    MPISET_BARRIER,
    MPISET_COLLECTIVES
} mpi_set_t;

typedef enum OMPTOptions {
    OMPTOOL_OPTS_NONE       = 0,
    /* OMPTOOL_OPTS_MPI        = 1 << 0, DEPRECATED */
    OMPTOOL_OPTS_BORROW     = 1 << 1,
    OMPTOOL_OPTS_LEND       = 1 << 2,
    /* OMPTOOL_OPTS_AGGRESSIVE = 0xF     DEPRECATED */
} omptool_opts_t;

typedef enum OMPTMVersion {
    OMPTM_NONE,
    OMPTM_OMP5,
    OMPTM_FREE_AGENTS,
    OMPTM_ROLE_SHIFT
} omptm_version_t;

static inline int min_int(int a, int b) { return a < b ? a : b; }
static inline int max_int(int a, int b) { return a > b ? a : b; }
static inline int64_t max_int64(int64_t a, int64_t b) { return a > b ? a : b; }

int  parse_bool(const char *str, bool *value);
bool equivalent_bool(const char *str1, const char *str2);

int  parse_negated_bool(const char *str, bool *value);
bool equivalent_negated_bool(const char *str1, const char *str2);

int  parse_int(const char *str, int *value);
bool equivalent_int(const char *str1, const char *str2);

/* verbose_opts_t */
int parse_verbose_opts(const char *str, verbose_opts_t *value);
const char* verbose_opts_tostr(verbose_opts_t value);
const char* get_verbose_opts_choices(void);
bool equivalent_verbose_opts(const char *str1, const char *str2);

/* verbose_fmt_t */
int parse_verbose_fmt(const char *str, verbose_fmt_t *value);
const char* verbose_fmt_tostr(verbose_fmt_t value);
const char* get_verbose_fmt_choices(void);
bool equivalent_verbose_fmt(const char *str1, const char *str2);

/* instrument_item_t */
int parse_instrument_items(const char *str, instrument_items_t *value);
const char* instrument_items_tostr(instrument_items_t value);
const char* get_instrument_items_choices(void);
bool equivalent_instrument_items(const char *str1, const char *str2);

/* debug_opts_t */
int parse_debug_opts(const char *str, debug_opts_t *value);
const char* debug_opts_tostr(debug_opts_t value);
const char* get_debug_opts_choices(void);
bool equivalent_debug_opts(const char *str1, const char *str2);

/* priority_t */
int parse_priority(const char *str, priority_t *value);
const char* priority_tostr(priority_t value);
const char* get_priority_choices(void);
bool equivalent_priority(const char *str1, const char *str2);

/* policy_t */
int parse_policy(const char *str, policy_t *value);
const char* policy_tostr(policy_t policy);
const char* get_policy_choices(void);
bool equivalent_policy(const char *str1, const char *str2);

/* talp_summary_t */
int parse_talp_summary(const char *str, talp_summary_t *value);
const char* talp_summary_tostr(talp_summary_t summary);
const char* get_talp_summary_choices(void);
bool equivalent_talp_summary(const char *str1, const char *str2);

/* interaction_mode_t */
int parse_mode(const char *str, interaction_mode_t *value);
const char* mode_tostr(interaction_mode_t value);
const char* get_mode_choices(void);
bool equivalent_mode(const char *str1, const char *str2);

/* mpi_set_t */
int parse_mpiset(const char *str, mpi_set_t *value);
const char* mpiset_tostr(mpi_set_t value);
const char* get_mpiset_choices(void);
bool equivalent_mpiset(const char *str1, const char *str2);

/* omptool_opts_t */
int parse_omptool_opts(const char *str, omptool_opts_t *value);
const char* omptool_opts_tostr(omptool_opts_t value);
const char* get_omptool_opts_choices(void);
bool equivalent_omptool_opts(const char *str1, const char *str2);

/* omptm_version_t */
int parse_omptm_version(const char *str, omptm_version_t *value);
const char* omptm_version_tostr(omptm_version_t value);
const char* get_omptm_version_choices(void);
bool equivalent_omptm_version_opts(const char *str1, const char *str2);

#endif /* TYPES_H */
