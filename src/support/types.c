/*********************************************************************************/
/*  Copyright 2009-2019 Barcelona Supercomputing Center                          */
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

#include "support/types.h"
#include "support/debug.h"
#include "apis/dlb_errors.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#define LINE_BREAK "\n                                             "

int parse_bool(const char *str, bool *value) {
    if (strcasecmp(str, "1")==0        ||
            strcasecmp(str, "yes")==0  ||
            strcasecmp(str, "true")==0) {
        *value = true;
    } else if (strcasecmp(str, "0")==0 ||
            strcasecmp(str, "no")==0   ||
            strcasecmp(str, "false")==0) {
        *value = false;
    } else {
        return DLB_ERR_NOENT;
    }
    return DLB_SUCCESS;
}

int parse_int(const char *str, int *value) {
    char *endptr;
    int val = strtol(str, &endptr, 0);
    if (errno == ERANGE || endptr == str) {
        return DLB_ERR_NOENT;
    }
    *value = val;
    return DLB_SUCCESS;
}


/* verbose_opts_t */
static const verbose_opts_t verbose_opts_values[] =
    {VB_API, VB_MICROLB, VB_SHMEM, VB_MPI_API, VB_MPI_INT, VB_STATS, VB_DROM, VB_ASYNC, VB_OMPT,
    VB_AFFINITY, VB_BARRIER, VB_TALP};
static const char* const verbose_opts_choices[] =
    {"api", "microlb", "shmem", "mpi_api", "mpi_intercept", "stats", "drom", "async", "ompt",
    "affinity", "barrier", "talp"};
static const char verbose_opts_choices_str[] =
    "api:microlb:shmem:mpi_api:mpi_intercept:stats:"LINE_BREAK
    "drom:async:ompt:affinity:barrier:talp";
enum { verbose_opts_nelems = sizeof(verbose_opts_values) / sizeof(verbose_opts_values[0]) };

int parse_verbose_opts(const char *str, verbose_opts_t *value) {
    *value = VB_CLEAR;
    int i;
    for (i=0; i<verbose_opts_nelems; ++i) {
        if (strstr(str, verbose_opts_choices[i]) != NULL) {
            *value |= verbose_opts_values[i];
        }
    }
    return DLB_SUCCESS;
}

const char* verbose_opts_tostr(verbose_opts_t value) {
    static char str[sizeof(verbose_opts_choices_str)] = "";
    char *p = str;
    int i;
    for (i=0; i<verbose_opts_nelems; ++i) {
        if (value & verbose_opts_values[i]) {
            if (p!=str) {
                *p = ':';
                ++p;
                *p = '\0';
            }
            p += sprintf(p, "%s", verbose_opts_choices[i]);
        }
    }
    return str;
}

const char* get_verbose_opts_choices(void) {
    return verbose_opts_choices_str;
}


/* verbose_fmt_t */
static const verbose_fmt_t verbose_fmt_values[] =
    {VBF_NODE, VBF_MPINODE, VBF_MPIRANK, VBF_SPID, VBF_THREAD, VBF_TSTAMP};
static const char* const verbose_fmt_choices[] =
    {"node", "mpinode", "mpirank", "spid", "thread", "timestamp"};
static const char verbose_fmt_choices_str[] =
    "node:mpinode:mpirank:spid:thread:timestamp";
enum { verbose_fmt_nelems = sizeof(verbose_fmt_values) / sizeof(verbose_fmt_values[0]) };

int parse_verbose_fmt(const char *str, verbose_fmt_t *value) {
    *value = VBF_CLEAR;
    int i;
    for (i=0; i<verbose_fmt_nelems; ++i) {
        if (strstr(str, verbose_fmt_choices[i]) != NULL) {
            *value |= verbose_fmt_values[i];
        }
    }
    return DLB_SUCCESS;
}

const char* verbose_fmt_tostr(verbose_fmt_t value) {
    static char str[sizeof(verbose_fmt_choices_str)] = "";
    char *p = str;
    int i;
    for (i=0; i<verbose_fmt_nelems; ++i) {
        if (value & verbose_fmt_values[i]) {
            if (p!=str) {
                *p = ':';
                ++p;
                *p = '\0';
            }
            p += sprintf(p, "%s", verbose_fmt_choices[i]);
        }
    }
    return str;
}

const char* get_verbose_fmt_choices(void) {
    return verbose_fmt_choices_str;
}


/* instrument_events_t */
static const instrument_events_t instrument_events_values[] =
    {INST_NONE, INST_ALL, INST_MPI, INST_LEWI, INST_DROM, INST_TALP, INST_BARR, INST_OMPT};
static const char* const instrument_events_choices[] =
    {"none", "all", "mpi", "lewi", "drom", "talp", "barrier", "ompt"};
static const char instrument_events_choices_str[] =
    "none:all:mpi:lewi:drom:talp:barrier:ompt";
enum { instrument_events_nelems = sizeof(instrument_events_values) / sizeof(instrument_events_values[0]) };

int parse_instrument_events(const char *str, instrument_events_t *value) {
    *value = INST_NONE;
    int i;
    for (i=0; i<instrument_events_nelems; ++i) {
        if (strstr(str, instrument_events_choices[i]) != NULL) {
            if (instrument_events_values[i] == INST_NONE) {
                *value = INST_NONE;
                break;
            }
            *value |= instrument_events_values[i];
        }
    }
    return DLB_SUCCESS;
}

const char* instrument_events_tostr(instrument_events_t value) {
    // particular cases
    if (value == INST_NONE) {
        return "none";
    }
    if (value == INST_ALL) {
        return "all";
    }

    static char str[sizeof(instrument_events_choices_str)] = "";
    char *p = str;
    int i;
    for (i=2; i<instrument_events_nelems; ++i) {
        if (value & instrument_events_values[i]) {
            if (p!=str) {
                *p = ':';
                ++p;
                *p = '\0';
            }
            p += sprintf(p, "%s", instrument_events_choices[i]);
        }
    }
    return str;
}

const char* get_instrument_events_choices(void) {
    return instrument_events_choices_str;
}


/* debug_opts_t */
static const debug_opts_t debug_opts_values[] =
    {DBG_RETURNSTOLEN, DBG_WERROR, DBG_LPOSTMORTEM};
static const char* const debug_opts_choices[] =
    {"return-stolen", "werror", "lend-post-mortem"};
static const char debug_opts_choices_str[] = "return-stolen:werror:lend-post-mortem";
enum { debug_opts_nelems = sizeof(debug_opts_values) / sizeof(debug_opts_values[0]) };

int parse_debug_opts(const char *str, debug_opts_t *value) {
    *value = DBG_CLEAR;
    int i;
    for (i=0; i<debug_opts_nelems; ++i) {
        if (strstr(str, debug_opts_choices[i]) != NULL) {
            *value |= debug_opts_values[i];
        }
    }
    return DLB_SUCCESS;
}

const char* debug_opts_tostr(debug_opts_t value) {
    static char str[sizeof(debug_opts_choices_str)] = "";
    char *p = str;
    int i;
    for (i=0; i<debug_opts_nelems; ++i) {
        if (value & debug_opts_values[i]) {
            if (p!=str) {
                *p = ':';
                ++p;
                *p = '\0';
            }
            p += sprintf(p, "%s", debug_opts_choices[i]);
        }
    }
    return str;
}

const char* get_debug_opts_choices(void) {
    return debug_opts_choices_str;
}


/* talp_summary_t */
static const talp_summary_t talp_summary_values[] =
    {SUMMARY_APP, SUMMARY_NODE, SUMMARY_PROCESS, SUMMARY_ITERATION, SUMMARY_OMP, SUMMARY_REGIONS};
static const char* const talp_summary_choices[] =
    {"app", "node", "process", "iteration", "omp","regions"};
static const char talp_summary_choices_str[] =
    "app:node:process:iteration:omp:regions";
enum { talp_summary_nelems = sizeof(talp_summary_values) / sizeof(talp_summary_values[0]) };

int parse_talp_summary(const char *str, talp_summary_t *value) {
    int i;
    for (i=0; i<talp_summary_nelems; ++i) {
        if (strstr(str, talp_summary_choices[i]) != NULL) {
            *value |= talp_summary_values[i];
        }
    }
    return DLB_SUCCESS;
}

const char* talp_summary_tostr(talp_summary_t value) {
    static char str[sizeof(talp_summary_choices_str)] = "";
    char *p = str;
    int i;
    for (i=0; i<talp_summary_nelems; ++i) {
        if (value & talp_summary_values[i]) {
            if (p!=str) {
                *p = ':';
                ++p;
                *p = '\0';
            }
            p += sprintf(p, "%s", talp_summary_choices[i]);
        }
    }
    return str;
}

const char* get_talp_summary_choices(void) {
    return talp_summary_choices_str;
}


/* priority_t */
static const priority_t priority_values[] =
    {PRIO_ANY, PRIO_NEARBY_FIRST, PRIO_NEARBY_ONLY, PRIO_SPREAD_IFEMPTY};
static const char* const priority_choices[] =
    {"any", "nearby-first", "nearby-only", "spread-ifempty"};
static const char priority_choices_str[] =
    "any, nearby-first, nearby-only, spread-ifempty";
enum { priority_nelems = sizeof(priority_values) / sizeof(priority_values[0]) };

int parse_priority(const char *str, priority_t *value) {
    int i;
    for (i=0; i<priority_nelems; ++i) {
        if (strcasecmp(str, priority_choices[i]) == 0) {
            *value = priority_values[i];
            return DLB_SUCCESS;
        }
    }
    return DLB_ERR_NOENT;
}

const char* priority_tostr(priority_t value) {
    int i;
    for (i=0; i<priority_nelems; ++i) {
        if (priority_values[i] == value) {
            return priority_choices[i];
        }
    }
    return "unknown";
}

const char* get_priority_choices(void) {
    return priority_choices_str;
}

/* policy_t */
static const policy_t policy_values[] = {POLICY_NONE, POLICY_LEWI, POLICY_LEWI_MASK};
static const char* const policy_choices[] = {"no", "LeWI", "LeWI_mask"};
static const char policy_choices_str[] = "no, LeWI, LeWI_mask";
enum { policy_nelems = sizeof(policy_values) / sizeof(policy_values[0]) };

int parse_policy(const char *str, policy_t *value) {
    int i;
    for (i=0; i<policy_nelems; ++i) {
        if (strcasecmp(str, policy_choices[i]) == 0) {
            *value = policy_values[i];
            return DLB_SUCCESS;
        }
    }
    return DLB_ERR_NOENT;
}

const char* policy_tostr(policy_t value) {
    int i;
    for (i=0; i<policy_nelems; ++i) {
        if (policy_values[i] == value) {
            return policy_choices[i];
        }
    }
    return "error";
}

const char* get_policy_choices(void) {
    return policy_choices_str;
}

/* interaction_mode_t */
static const interaction_mode_t mode_values[] = {MODE_POLLING, MODE_ASYNC};
static const char* const mode_choices[] = {"polling", "async"};
static const char mode_choices_str[] = "polling, async";
enum { mode_nelems = sizeof(mode_values) / sizeof(mode_values[0]) };

int parse_mode(const char *str, interaction_mode_t *value) {
    int i;
    for (i=0; i<mode_nelems; ++i) {
        if (strcasecmp(str, mode_choices[i]) == 0) {
            *value = mode_values[i];
            return DLB_SUCCESS;
        }
    }
    return DLB_ERR_NOENT;
}

const char* mode_tostr(interaction_mode_t value) {
    int i;
    for (i=0; i<mode_nelems; ++i) {
        if (mode_values[i] == value) {
            return mode_choices[i];
        }
    }
    return "unknown";
}

const char* get_mode_choices(void) {
    return mode_choices_str;
}

/* mpi_set_t */
static const mpi_set_t mpiset_values[] = {MPISET_ALL, MPISET_BARRIER, MPISET_COLLECTIVES};
static const char* const mpiset_choices[] = {"all", "barrier", "collectives"};
static const char mpiset_choices_str[] = "all, barrier, collectives";
enum { mpiset_nelems = sizeof(mpiset_values) / sizeof(mpiset_values[0]) };

int parse_mpiset(const char *str, mpi_set_t *value) {
    int i;
    for (i=0; i<mpiset_nelems; ++i) {
        if (strcasecmp(str, mpiset_choices[i]) == 0) {
            *value = mpiset_values[i];
            return DLB_SUCCESS;
        }
    }
    return DLB_ERR_NOENT;
}

const char* mpiset_tostr(mpi_set_t value) {
    int i;
    for (i=0; i<mpiset_nelems; ++i) {
        if (mpiset_values[i] == value) {
            return mpiset_choices[i];
        }
    }
    return "unknown";
}

const char* get_mpiset_choices(void) {
    return mpiset_choices_str;
}

/* ompt_opts_t */
static const ompt_opts_t ompt_opts_values[] = {OMPT_OPTS_MPI, OMPT_OPTS_BORROW, OMPT_OPTS_LEND};
static const char* const ompt_opts_choices[] = {"mpi", "borrow", "lend"};
static const char ompt_opts_choices_str[] = "mpi, borrow, lend";
enum { ompt_opts_nelems = sizeof(ompt_opts_values) / sizeof(ompt_opts_values[0]) };

int parse_ompt_opts(const char *str, ompt_opts_t *value) {
    *value = OMPT_OPTS_CLEAR;
    int i;
    for (i=0; i<ompt_opts_nelems; ++i) {
        if (strstr(str, ompt_opts_choices[i]) != NULL) {
            *value |= ompt_opts_values[i];
        }
    }
    return DLB_SUCCESS;
}

const char* ompt_opts_tostr(ompt_opts_t value) {
    static char str[sizeof(ompt_opts_choices_str)] = "";
    char *p = str;
    int i;
    for (i=0; i<ompt_opts_nelems; ++i) {
        if (value & ompt_opts_values[i]) {
            if (p!=str) {
                *p = ':';
                ++p;
                *p = '\0';
            }
            p += sprintf(p, "%s", ompt_opts_choices[i]);
        }
    }
    return str;
}

const char* get_ompt_opts_choices(void) {
    return ompt_opts_choices_str;
}
