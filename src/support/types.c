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

#include "support/types.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void parse_bool(const char *str, bool *value) {
    *value = false;
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

/* blocking_mode_t */
static const blocking_mode_t blocking_mode_values[] = {ONE_CPU, BLOCK};
static const char* const blocking_mode_choices[] = {"1CPU", "BLOCK"};
static const char blocking_mode_choices_str[] = "1CPU, BLOCK";
enum { blocking_mode_nelems = sizeof(blocking_mode_values) / sizeof(blocking_mode_values[0]) };

void parse_blocking_mode(const char *str, blocking_mode_t *value) {
    *value = ONE_CPU;
    int i;
    for (i=0; i<blocking_mode_nelems; ++i) {
        if (strcasecmp(str, blocking_mode_choices[i]) == 0) {
            *value = blocking_mode_values[i];
            break;
        }
    }
}

const char* blocking_mode_tostr(blocking_mode_t value) {
    int i;
    for (i=0; i<blocking_mode_nelems; ++i) {
        if (blocking_mode_values[i] == value) {
            return blocking_mode_choices[i];
        }
    }
    return "unknown";
}

const char* get_blocking_mode_choices(void) {
    return blocking_mode_choices_str;
}


/* verbose_opts_t */
static const verbose_opts_t verbose_opts_values[] =
    {VB_API, VB_MICROLB, VB_SHMEM, VB_MPI_API, VB_MPI_INT, VB_STATS, VB_DROM};
static const char* const verbose_opts_choices[] =
    {"api", "microlb", "shmem", "mpi_api", "mpi_intercept", "stats", "drom"};
static const char verbose_opts_choices_str[] =
    "api:microlb:shmem:mpi_api:mpi_intercept:stats:drom";
enum { verbose_opts_nelems = sizeof(verbose_opts_values) / sizeof(verbose_opts_values[0]) };

void parse_verbose_opts(const char *str, verbose_opts_t *value) {
    *value = VB_CLEAR;
    int i;
    for (i=0; i<verbose_opts_nelems; ++i) {
        if (strstr(str, verbose_opts_choices[i]) != NULL) {
            *value |= verbose_opts_values[i];
        }
    }
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
    {VBF_NODE, VBF_PID, VBF_MPINODE, VBF_MPIRANK, VBF_THREAD};
static const char* const verbose_fmt_choices[] =
    {"node", "pid", "mpinode", "mpirank", "thread"};
static const char verbose_fmt_choices_str[] =
    "node:pid:mpinode:mpirank:thread";
enum { verbose_fmt_nelems = sizeof(verbose_fmt_values) / sizeof(verbose_fmt_values[0]) };

void parse_verbose_fmt(const char *str, verbose_fmt_t *value) {
    *value = VBF_CLEAR;
    int i;
    for (i=0; i<verbose_fmt_nelems; ++i) {
        if (strstr(str, verbose_fmt_choices[i]) != NULL) {
            *value |= verbose_fmt_values[i];
        }
    }
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


/* debug_opts_t */
static const debug_opts_t debug_opts_values[] = {DBG_REGSIGNALS, DBG_RETURNSTOLEN};
static const char* const debug_opts_choices[] = {"register-signals", "return-stolen"};
static const char debug_opts_choices_str[] = "register-signals:return-stolen";
enum { debug_opts_nelems = sizeof(debug_opts_values) / sizeof(debug_opts_values[0]) };

void parse_debug_opts(const char *str, debug_opts_t *value) {
    *value = DBG_CLEAR;
    int i;
    for (i=0; i<debug_opts_nelems; ++i) {
        if (strstr(str, debug_opts_choices[i]) != NULL) {
            *value |= debug_opts_values[i];
        }
    }
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


/* priority_t */
static const priority_t priority_values[] =
    {PRIO_NONE, PRIO_AFFINITY_FIRST, PRIO_AFFINITY_FULL, PRIO_AFFINITY_ONLY};
static const char* const priority_choices[] =
    {"none", "affinity_first", "affinity_full", "affinity_only"};
static const char priority_choices_str[] =
    "none, affinity_first, affinity_full, affinity_only";
enum { priority_nelems = sizeof(priority_values) / sizeof(priority_values[0]) };

void parse_priority(const char *str, priority_t *value) {
    *value = PRIO_AFFINITY_FIRST;
    int i;
    for (i=0; i<priority_nelems; ++i) {
        if (strcasecmp(str, priority_choices[i]) == 0) {
            *value = priority_values[i];
            break;
        }
    }
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
static const policy_t policy_values[] =
    {POLICY_NONE, POLICY_LEWI, POLICY_WEIGHT, POLICY_LEWI_MASK, POLICY_AUTO_LEWI_MASK, POLICY_NEW};
static const char* const policy_choices[] =
    {"no", "LeWI", "WEIGHT", "LeWI_mask", "auto_LeWI_mask", "new"};
static const char policy_choices_str[] =
    "no, LeWI, WEIGHT, LeWI_mask, auto_LeWI_mask, new";
enum { policy_nelems = sizeof(policy_values) / sizeof(policy_values[0]) };

void parse_policy(const char *str, policy_t *value) {
    *value = POLICY_NONE;
    int i;
    for (i=0; i<policy_nelems; ++i) {
        if (strcasecmp(str, policy_choices[i]) == 0) {
            *value = policy_values[i];
            break;
        }
    }
}

const char* policy_tostr(policy_t policy) {
    switch(policy) {
        case POLICY_NONE: return "None";
        case POLICY_LEWI: return "LeWI";
        case POLICY_WEIGHT: return "Weight";
        case POLICY_LEWI_MASK: return "LeWI mask";
        case POLICY_AUTO_LEWI_MASK: return "Autonomous LeWI mask";
        case POLICY_NEW: return "New (WIP)";
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

void parse_mode(const char *str, interaction_mode_t *value) {
    *value = MODE_POLLING;
    int i;
    for (i=0; i<mode_nelems; ++i) {
        if (strcasecmp(str, mode_choices[i]) == 0) {
            *value = mode_values[i];
            break;
        }
    }
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
