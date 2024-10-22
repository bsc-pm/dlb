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

#include "support/types.h"
#include "support/debug.h"
#include "apis/dlb_errors.h"

#include <limits.h>
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

bool equivalent_bool(const char *str1, const char *str2) {
    bool b1 = false;
    bool b2 = true;
    int err1 = parse_bool(str1, &b1);
    int err2 = parse_bool(str2, &b2);
    return err1 == DLB_SUCCESS && err2 == DLB_SUCCESS && b1 == b2;
}

int parse_negated_bool(const char *str, bool *value) {
    if (strcasecmp(str, "1")==0        ||
            strcasecmp(str, "yes")==0  ||
            strcasecmp(str, "true")==0) {
        *value = false;
    } else if (strcasecmp(str, "0")==0 ||
            strcasecmp(str, "no")==0   ||
            strcasecmp(str, "false")==0) {
        *value = true;
    } else {
        return DLB_ERR_NOENT;
    }
    return DLB_SUCCESS;
}

bool equivalent_negated_bool(const char *str1, const char *str2) {
    bool b1 = false;
    bool b2 = true;
    int err1 = parse_negated_bool(str1, &b1);
    int err2 = parse_negated_bool(str2, &b2);
    return err1 == DLB_SUCCESS && err2 == DLB_SUCCESS && b1 == b2;
}

int parse_int(const char *str, int *value) {
    char *endptr;
    long val = strtol(str, &endptr, 0);
    if ((val == 0 && endptr == str)
            || ((val == LONG_MIN || val == LONG_MAX) && errno == ERANGE)) {
        debug_warning("Error parsing integer. str: %s, strlen: %lu, val: %ld, "
                "errno: %d, strptr: %p, endptr: %p", str, strlen(str), val,
                errno, str, endptr);
        return DLB_ERR_NOENT;
    }
    *value = (int)val;
    return DLB_SUCCESS;
}

bool equivalent_int(const char *str1, const char *str2) {
    int i1 = 0;
    int i2 = 1;
    int err1 = parse_int(str1, &i1);
    int err2 = parse_int(str2, &i2);
    return err1 == DLB_SUCCESS && err2 == DLB_SUCCESS && i1 == i2;
}


/* verbose_opts_t */
static const verbose_opts_t verbose_opts_values[] =
    {VB_CLEAR, VB_ALL, VB_API, VB_MICROLB, VB_SHMEM, VB_MPI_API, VB_MPI_INT, VB_STATS,
        VB_DROM, VB_ASYNC, VB_OMPT, VB_AFFINITY, VB_BARRIER, VB_TALP, VB_INSTR};
static const char* const verbose_opts_choices[] =
    {"no", "all", "api", "microlb", "shmem", "mpi_api", "mpi_intercept", "stats", "drom",
        "async", "ompt", "affinity", "barrier", "talp", "instrument"};
static const char verbose_opts_choices_str[] =
    "no:all:api:microlb:shmem:mpi_api:mpi_intercept:stats:"LINE_BREAK
    "drom:async:ompt:affinity:barrier:talp:instrument";
enum { verbose_opts_nelems = sizeof(verbose_opts_values) / sizeof(verbose_opts_values[0]) };

int parse_verbose_opts(const char *str, verbose_opts_t *value) {
    /* particular case: '--verbose/--verbose=yes' enables all verbose options */
    if (strcmp(str, "yes") == 0) {
        *value = VB_ALL;
        return DLB_SUCCESS;
    }

    *value = VB_CLEAR;

    /* tokenize multiple options separated by ':' */
    char *end_token = NULL;
    size_t len = strlen(str) + 1;
    char *str_copy = malloc(sizeof(char)*len);
    strcpy(str_copy, str);
    char *token = strtok_r(str_copy, ":", &end_token);
    while (token) {
        int i;
        for (i=0; i<verbose_opts_nelems; ++i) {
            if (strcmp(token, verbose_opts_choices[i]) == 0) {
                *value |= verbose_opts_values[i];
                break;
            }
        }
        if (i == verbose_opts_nelems) {
            warning("Unknown --verbose option: %s", token);
        }
        token = strtok_r(NULL, ":", &end_token);
    }
    free(str_copy);

    return DLB_SUCCESS;
}

const char* verbose_opts_tostr(verbose_opts_t value) {
    /* particular cases */
    if (value == VB_CLEAR) {
        return "no";
    }
    if (value == VB_ALL) {
        return "all";
    }

    static char str[sizeof(verbose_opts_choices_str)] = "";
    char *p = str;
    int i;
    for (i=2; i<verbose_opts_nelems; ++i) {
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

bool equivalent_verbose_opts(const char *str1, const char *str2) {
    verbose_opts_t value1, value2;
    parse_verbose_opts(str1, &value1);
    parse_verbose_opts(str2, &value2);
    return value1 == value2;
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

    /* tokenize multiple options separated by ':' */
    char *end_token = NULL;
    size_t len = strlen(str) + 1;
    char *str_copy = malloc(sizeof(char)*len);
    strcpy(str_copy, str);
    char *token = strtok_r(str_copy, ":", &end_token);
    while (token) {
        int i;
        for (i=0; i<verbose_fmt_nelems; ++i) {
            if (strcmp(token, verbose_fmt_choices[i]) == 0) {
                *value |= verbose_fmt_values[i];
                break;
            }
        }
        if (i == verbose_fmt_nelems) {
            warning("Unknown --verbose-format option: %s", token);
        }
        token = strtok_r(NULL, ":", &end_token);
    }
    free(str_copy);

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

bool equivalent_verbose_fmt(const char *str1, const char *str2) {
    verbose_fmt_t value1, value2;
    parse_verbose_fmt(str1, &value1);
    parse_verbose_fmt(str2, &value2);
    return value1 == value2;
}


/* instrument_items_t */
static const instrument_items_t instrument_items_values[] =
    {INST_NONE, INST_ALL, INST_MPI, INST_LEWI, INST_DROM, INST_TALP, INST_BARR, INST_OMPT, INST_CPUS, INST_CBCK};
static const char* const instrument_items_choices[] =
    {"none", "all", "mpi", "lewi", "drom", "talp", "barrier", "ompt", "cpus", "callbacks"};
static const char instrument_items_choices_str[] =
    "none:all:mpi:lewi:drom:talp:barrier:ompt:cpus:callbacks";
enum { instrument_items_nelems = sizeof(instrument_items_values) / sizeof(instrument_items_values[0]) };

int parse_instrument_items(const char *str, instrument_items_t *value) {
    /* particular case: '--instrument/--instrument=yes' enables all instrument items */
    if (strcmp(str, "yes") == 0) {
        *value = INST_ALL;
        return DLB_SUCCESS;
    }

    /* particular case: '--no-instrument/--instrument=no' disables all instrument items */
    if (strcmp(str, "no") == 0) {
        *value = INST_NONE;
        return DLB_SUCCESS;
    }

    *value = INST_NONE;

    /* tokenize multiple options separated by ':' */
    char *end_token = NULL;
    size_t len = strlen(str) + 1;
    char *str_copy = malloc(sizeof(char)*len);
    strcpy(str_copy, str);
    char *token = strtok_r(str_copy, ":", &end_token);
    while (token) {
        int i;
        for (i=0; i<instrument_items_nelems; ++i) {
            if (strcmp(token, instrument_items_choices[i]) == 0) {
                *value |= instrument_items_values[i];
                break;
            }
        }
        if (i == instrument_items_nelems) {
            warning("Unknown --instrument option: %s", token);
        }
        token = strtok_r(NULL, ":", &end_token);
    }
    free(str_copy);

    return DLB_SUCCESS;
}

const char* instrument_items_tostr(instrument_items_t value) {
    // particular cases
    if (value == INST_NONE) {
        return "none";
    }
    if (value == INST_ALL) {
        return "all";
    }

    static char str[sizeof(instrument_items_choices_str)] = "";
    char *p = str;
    int i;
    for (i=2; i<instrument_items_nelems; ++i) {
        if (value & instrument_items_values[i]) {
            if (p!=str) {
                *p = ':';
                ++p;
                *p = '\0';
            }
            p += sprintf(p, "%s", instrument_items_choices[i]);
        }
    }
    return str;
}

const char* get_instrument_items_choices(void) {
    return instrument_items_choices_str;
}

bool equivalent_instrument_items(const char *str1, const char *str2) {
    instrument_items_t value1, value2;
    parse_instrument_items(str1, &value1);
    parse_instrument_items(str2, &value2);
    return value1 == value2;
}


/* debug_opts_t */
static const debug_opts_t debug_opts_values[] =
    {DBG_RETURNSTOLEN, DBG_WERROR, DBG_LPOSTMORTEM, DBG_WARNMPI};
static const char* const debug_opts_choices[] =
    {"return-stolen", "werror", "lend-post-mortem", "warn-mpi-version"};
static const char debug_opts_choices_str[] = "return-stolen:werror:lend-post-mortem:warn-mpi-version";
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

bool equivalent_debug_opts(const char *str1, const char *str2) {
    debug_opts_t value1, value2;
    parse_debug_opts(str1, &value1);
    parse_debug_opts(str2, &value2);
    return value1 == value2;
}


/* talp_summary_t */
static const talp_summary_t talp_summary_values[] =
    {SUMMARY_NONE, SUMMARY_ALL, SUMMARY_POP_METRICS, SUMMARY_POP_RAW, SUMMARY_NODE,
        SUMMARY_PROCESS};
static const char* const talp_summary_choices[] =
    {"none", "all", "pop-metrics", "pop-raw", "node", "process"};
static const char talp_summary_choices_str[] =
    "none:all:pop-metrics:pop-raw:node:process";
enum { talp_summary_nelems = sizeof(talp_summary_values) / sizeof(talp_summary_values[0]) };

int parse_talp_summary(const char *str, talp_summary_t *value) {
    /* particular case: '--talp-summary/--talp-summary=yes' enables only POP metrics */
    if (strcmp(str, "yes") == 0) {
        *value = SUMMARY_POP_METRICS;
        return DLB_SUCCESS;
    }

    /* particular case: '--no-talp-summary/--talp-summary=no' disables all summaries */
    if (strcmp(str, "no") == 0) {
        *value = SUMMARY_NONE;
        return DLB_SUCCESS;
    }

    *value = SUMMARY_NONE;

    /* tokenize multiple options separated by ':' */
    char *end_token = NULL;
    size_t len = strlen(str) + 1;
    char *str_copy = malloc(sizeof(char)*len);
    strcpy(str_copy, str);
    char *token = strtok_r(str_copy, ":", &end_token);
    while (token) {
        int i;
        for (i=0; i<talp_summary_nelems; ++i) {
            if (strcmp(token, talp_summary_choices[i]) == 0) {
                *value |= talp_summary_values[i];
                break;
            }

            /* Support deprecated values */
            if (strcmp(token, "app") == 0) {
                *value |= SUMMARY_POP_METRICS;
                break;
            }
        }
        if (i == talp_summary_nelems) {
            warning("Unknown --talp-summary option: %s", token);
        }
        token = strtok_r(NULL, ":", &end_token);
    }
    free(str_copy);

    return DLB_SUCCESS;
}

const char* talp_summary_tostr(talp_summary_t value) {
    // particular cases
    if (value == SUMMARY_NONE) {
        return "none";
    }
    if (value == SUMMARY_ALL) {
        return "all";
    }

    static char str[sizeof(talp_summary_choices_str)] = "";
    char *p = str;
    int i;
    for (i=2; i<talp_summary_nelems; ++i) {
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

bool equivalent_talp_summary(const char *str1, const char *str2) {
    talp_summary_t value1, value2;
    parse_talp_summary(str1, &value1);
    parse_talp_summary(str2, &value2);
    return value1 == value2;
}


/* lewi_affinity_t */
static const lewi_affinity_t lewi_affinity_values[] =
    {LEWI_AFFINITY_AUTO, LEWI_AFFINITY_NONE, LEWI_AFFINITY_MASK,
        LEWI_AFFINITY_NEARBY_FIRST, LEWI_AFFINITY_NEARBY_ONLY, LEWI_AFFINITY_SPREAD_IFEMPTY};
static const char* const lewi_affinity_choices[] =
    {"auto", "none", "mask", "nearby-first", "nearby-only", "spread-ifempty"};
static const char lewi_affinity_choices_str[] =
    "auto, none, mask, nearby-first,"LINE_BREAK
    "nearby-only, spread-ifempty";
enum { lewi_affinity_nelems = sizeof(lewi_affinity_values) / sizeof(lewi_affinity_values[0]) };

int parse_lewi_affinity(const char *str, lewi_affinity_t *value) {

    for (int i = 0; i < lewi_affinity_nelems; ++i) {
        if (strcasecmp(str, lewi_affinity_choices[i]) == 0) {
            *value = lewi_affinity_values[i];
            return DLB_SUCCESS;
        }
    }

    /* Support deprecated values */
    if (strcasecmp(str, "any") == 0) {
        *value = LEWI_AFFINITY_MASK;
        return DLB_SUCCESS;
    }

    return DLB_ERR_NOENT;
}

const char* lewi_affinity_tostr(lewi_affinity_t value) {
    int i;
    for (i=0; i<lewi_affinity_nelems; ++i) {
        if (lewi_affinity_values[i] == value) {
            return lewi_affinity_choices[i];
        }
    }
    return "unknown";
}

const char* get_lewi_affinity_choices(void) {
    return lewi_affinity_choices_str;
}

bool equivalent_lewi_affinity(const char *str1, const char *str2) {
    lewi_affinity_t value1 = LEWI_AFFINITY_NONE;
    lewi_affinity_t value2 = LEWI_AFFINITY_MASK;
    int err1 = parse_lewi_affinity(str1, &value1);
    int err2 = parse_lewi_affinity(str2, &value2);
    return err1 == DLB_SUCCESS && err2 == DLB_SUCCESS && value1 == value2;
}

/* policy_t: most of this stuff is depcrecated, only policy_tostr is still used */
static const policy_t policy_values[] = {POLICY_NONE, POLICY_LEWI, POLICY_LEWI_ASYNC, POLICY_LEWI_MASK};
static const char* const policy_choices[] = {"no", "LeWI", "LeWI_async", "LeWI_mask"};
static const char policy_choices_str[] = "no, LeWI, LeWI_async, LeWI_mask";
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

bool equivalent_policy(const char *str1, const char *str2) {
    policy_t value1 = POLICY_NONE;
    policy_t value2 = POLICY_LEWI;
    int err1 = parse_policy(str1, &value1);
    int err2 = parse_policy(str2, &value2);
    return err1 == DLB_SUCCESS && err2 == DLB_SUCCESS && value1 == value2;
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

bool equivalent_mode(const char *str1, const char *str2) {
    interaction_mode_t value1 = MODE_POLLING;
    interaction_mode_t value2 = MODE_ASYNC;
    int err1 = parse_mode(str1, &value1);
    int err2 = parse_mode(str2, &value2);
    return err1 == DLB_SUCCESS && err2 == DLB_SUCCESS && value1 == value2;
}

/* mpi_set_t */
static const mpi_set_t mpiset_values[] = {MPISET_NONE, MPISET_ALL, MPISET_BARRIER, MPISET_COLLECTIVES};
static const char* const mpiset_choices[] = {"none", "all", "barrier", "collectives"};
static const char mpiset_choices_str[] = "none, all, barrier, collectives";
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

bool equivalent_mpiset(const char *str1, const char *str2) {
    mpi_set_t value1 = MPISET_NONE;
    mpi_set_t value2 = MPISET_ALL;
    int err1 = parse_mpiset(str1, &value1);
    int err2 = parse_mpiset(str2, &value2);
    return err1 == DLB_SUCCESS && err2 == DLB_SUCCESS && value1 == value2;
}

/* omptool_opts_t */
static const omptool_opts_t omptool_opts_values[] =
    {OMPTOOL_OPTS_NONE, OMPTOOL_OPTS_BORROW, OMPTOOL_OPTS_LEND};
static const char* const omptool_opts_choices[] = {"none", "borrow", "lend"};
static const char omptool_opts_choices_str[] = "none, {borrow:lend}";
enum { omptool_opts_nelems = sizeof(omptool_opts_values) / sizeof(omptool_opts_values[0]) };

int parse_omptool_opts(const char *str, omptool_opts_t *value) {

    *value = OMPTOOL_OPTS_NONE;

    /* tokenize multiple options separated by ':' */
    char *end_token = NULL;
    size_t len = strlen(str) + 1;
    char *str_copy = malloc(sizeof(char)*len);
    strcpy(str_copy, str);
    char *token = strtok_r(str_copy, ":", &end_token);
    while (token) {
        int i;
        for (i=0; i<omptool_opts_nelems; ++i) {
            if (strcmp(token, omptool_opts_choices[i]) == 0) {
                *value |= omptool_opts_values[i];
                break;
            }
        }
        if (i == omptool_opts_nelems) {
            warning("Unknown --lewi-ompt option: %s", token);
        }
        token = strtok_r(NULL, ":", &end_token);
    }
    free(str_copy);

    return DLB_SUCCESS;
}

const char* omptool_opts_tostr(omptool_opts_t value) {
    static char str[sizeof(omptool_opts_choices_str)] = "";
    char *p = str;
    int i;
    for (i=0; i<omptool_opts_nelems; ++i) {
        if (value & omptool_opts_values[i]) {
            if (p!=str) {
                *p = ':';
                ++p;
                *p = '\0';
            }
            p += sprintf(p, "%s", omptool_opts_choices[i]);
        }
    }
    return str;
}

const char* get_omptool_opts_choices(void) {
    return omptool_opts_choices_str;
}

bool equivalent_omptool_opts(const char *str1, const char *str2) {
    omptool_opts_t value1, value2;
    parse_omptool_opts(str1, &value1);
    parse_omptool_opts(str2, &value2);
    return value1 == value2;
}

/* omptm_version_t */
static const omptm_version_t omptm_version_values[] = {OMPTM_NONE, OMPTM_OMP5,
    OMPTM_FREE_AGENTS, OMPTM_ROLE_SHIFT};
static const char* const omptm_version_choices[] = {"none", "omp5", "free-agents", "role-shift"};
static const char omptm_version_choices_str[] = "none, omp5, free-agents, role-shift";
enum { omptm_version_nelems = sizeof(omptm_version_values) / sizeof(omptm_version_values[0]) };

int parse_omptm_version(const char *str, omptm_version_t *value) {
    int i;
    for (i=0; i<omptm_version_nelems; ++i) {
        if (strcasecmp(str, omptm_version_choices[i]) == 0) {
            *value = omptm_version_values[i];
            return DLB_SUCCESS;
        }
    }
    return DLB_ERR_NOENT;
}

const char* omptm_version_tostr(omptm_version_t value) {
    int i;
    for (i=0; i<omptm_version_nelems; ++i) {
        if (omptm_version_values[i] == value) {
            return omptm_version_choices[i];
        }
    }
    return "unknown";
}

const char* get_omptm_version_choices(void) {
    return omptm_version_choices_str;
}

bool equivalent_omptm_version_opts(const char *str1, const char *str2) {
    omptm_version_t value1 = OMPTM_OMP5;
    omptm_version_t value2 = OMPTM_FREE_AGENTS;
    int err1 = parse_omptm_version(str1, &value1);
    int err2 = parse_omptm_version(str2, &value2);
    return err1 == DLB_SUCCESS && err2 == DLB_SUCCESS && value1 == value2;
}

