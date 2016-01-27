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

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "support/types.h"
#include "support/debug.h"

#define MAX_OPTIONS 32
#define MAX_OPTION_LENGTH 32
#define MAX_DESCRIPTION 1024


// Options storage and internal getters
static char opt_policy[MAX_OPTION_LENGTH];
const char* options_get_policy(void) { return opt_policy; }

static bool opt_statistics;
bool options_get_statistics(void) { return opt_statistics; }

static bool opt_drom;
bool options_get_drom(void) { return opt_drom; }

static bool opt_just_barrier;
bool options_get_just_barier(void) { return opt_just_barrier; }

static blocking_mode_t opt_lend_mode;
blocking_mode_t options_get_lend_mode(void) { return opt_lend_mode; }

static verbose_opts_t opt_verbose;
verbose_opts_t options_get_verbose(void) { return opt_verbose; }

static verbose_fmt_t opt_verbose_fmt;
verbose_fmt_t options_get_verbose_fmt(void) { return opt_verbose_fmt; }

static bool opt_trace_enabled;
bool options_get_trace_enabled(void) { return opt_trace_enabled; }

static bool opt_trace_counters;
bool options_get_trace_counters(void) { return opt_trace_counters; }

static char opt_mask[MAX_OPTION_LENGTH];
const char* options_get_mask(void) { return opt_mask; }

static bool opt_greedy;
bool options_get_greedy(void) { return opt_greedy; }

static char opt_shm_key[MAX_OPTION_LENGTH];
const char* options_get_shm_key(void) { return opt_shm_key; }

static bool opt_bind;
bool options_get_bind(void) { return opt_bind; }

static char opt_thread_distribution[MAX_OPTION_LENGTH];
const char* options_get_thread_distribution(void) { return opt_thread_distribution; }

static bool opt_aggressive_init;
bool options_get_aggressive_init(void) { return opt_aggressive_init; }

static bool opt_prioritize_locality;
bool options_get_priorize_locality(void) { return opt_prioritize_locality; }


typedef enum OptionTypes {
    OPT_BOOL_T,
    OPT_INT_T,
    OPT_STR_T,
    OPT_BLCK_T,     // blocking_mode_t
    OPT_VB_T,       // verbose_opts_t
    OPT_VBFMT_T     // verbose_fmt_t
} option_type_t;

typedef struct {
    char            var_name[MAX_OPTION_LENGTH];    // LB_OPTION
    char            arg_name[MAX_OPTION_LENGTH];    // --option
    char            description[MAX_DESCRIPTION];
    option_type_t   type;
    void            *value;
    bool            readonly;
    bool            optional;
} option_t;

static option_t **options = NULL;
static int num_options = 0;
static char *lb_args = NULL;



/* Parse LB_ARGS and remove argument if found */
static char* parse_env_arg(const char *arg_name) {
    static char value[MAX_OPTION_LENGTH];
    value[0] = 0;
    if (lb_args && arg_name) {
        // Tokenize a copy of lb_args with " " delimiter
        char *end_space;
        char *lb_args_copy = (char*)malloc(1+sizeof(char)*strlen(lb_args));
        strncpy(lb_args_copy, lb_args, strlen(lb_args));
        char *token = strtok_r(lb_args_copy, " ", &end_space);
        while (token) {
            // Break token into two tokens "--argument" = "value"
            char *end_equal;
            char *current_arg = strtok_r(token, "=", &end_equal);
            fatal_cond(!current_arg, "Bad format parsing LB_ARGS: --argument=value");

            if (strcmp(current_arg, arg_name) == 0) {
                // Get value to return
                char *val = strtok_r(NULL, "=", &end_equal);
                fatal_cond(!val, "Bad format parsing LB_ARGS: --argument=value");
                strncpy(value, val, MAX_OPTION_LENGTH);

                // Remove "token=value" from lb_args
                char *dest = strstr(lb_args, token);
                char *src = dest+strlen(token)+1+strlen(value);
                size_t n = 1+strlen(dest+strlen(token)+1+strlen(value));
                memmove(dest, src, n);

                break;
            }
            // next token
            token = strtok_r(NULL, " ", &end_space);
        }
        free(lb_args_copy);
    }
    return (value[0]==0) ? NULL : value;
}

static option_t* register_option(const char *var, const char *arg, option_type_t type,
                                void *value, bool readonly, bool optional,
                                const void *default_value, const char *desc ) {

    option_t *new_option = (option_t*) malloc(sizeof(option_t));
    strncpy(new_option->var_name, var, MAX_OPTION_LENGTH);
    strncpy(new_option->arg_name, arg, MAX_OPTION_LENGTH);
    strncpy(new_option->description, desc, MAX_DESCRIPTION);
    new_option->type = type;
    new_option->value = value;
    new_option->readonly = readonly;
    new_option->optional = optional;

    // LB_ARGS takes precedence over LB_var
    char *user_value = parse_env_arg(arg);
    if (!user_value) user_value = getenv(var);

    switch(type) {
        case(OPT_BOOL_T):
            if (user_value) {
                parse_bool(user_value, (bool*)new_option->value);
            } else if (default_value) {
                *(bool*)new_option->value = *(bool*)default_value;
            }
            break;
        case(OPT_INT_T):
            if (user_value) {
                parse_int(user_value, (int*)new_option->value);
            } else if (default_value) {
                *(int*)new_option->value = *(int*)default_value;
            }
            break;
        case(OPT_STR_T):
            if (user_value) {
                strncpy(new_option->value, user_value, MAX_OPTION_LENGTH);
            } else if (default_value) {
                strncpy(new_option->value, default_value, MAX_OPTION_LENGTH);
            }
            break;
        case(OPT_BLCK_T):
            if (user_value) {
                parse_blocking_mode(user_value, (blocking_mode_t*)new_option->value);
            } else if (default_value) {
                parse_blocking_mode(default_value, (blocking_mode_t*)new_option->value);
            }
            break;
        case(OPT_VB_T):
            if (user_value) {
                parse_verbose_opts(user_value, (verbose_opts_t*)new_option->value);
            } else if (default_value) {
                parse_verbose_opts(default_value, (verbose_opts_t*)new_option->value);
            }
            break;
        case(OPT_VBFMT_T):
            if (user_value) {
                parse_verbose_fmt(user_value, (verbose_fmt_t*)new_option->value);
            } else if (default_value) {
                parse_verbose_fmt(default_value, (verbose_fmt_t*)new_option->value);
            }
            break;
    }

    fatal_cond(!optional && !new_option->value,
            "Variable %s must be defined", new_option->var_name);

    ++num_options;
    return new_option;
}

void options_init(void) {
    const bool RO = true;
    const bool RW = false;
    const bool OPTIONAL = true;
    const bool FALSE = false;
    const bool TRUE = true;

    // Copy LB_ARGS env. variable
    char *env_lb_args = getenv("LB_ARGS");
    if (env_lb_args) {
        size_t len = strlen(env_lb_args);
        lb_args = (char*)malloc(1+sizeof(char)*len);
        strncpy(lb_args, env_lb_args, len);
    }

    // Fill options array
    options = (option_t**) malloc(sizeof(option_t*)*MAX_OPTIONS);

    // modules
    int i = 0;
    options[i++] = register_option("LB_POLICY", "--policy",
            OPT_STR_T, &opt_policy, RO, OPTIONAL, "no",
            "Lend Balancing Policy. Choose one of: \n"
            " - No: No policy\n"
            " - LeWI: Lend When Idle, basic policy\n"
            " - LeWI_mask: LeWI with mask support\n"
            " - auto_LeWI_mask: autonomous LeWI. To be used with highly malleable runtimes\n"
            " - RaL:");

    options[i++] = register_option("LB_STATISTICS", "--statistics",
            OPT_BOOL_T, &opt_statistics, RO, OPTIONAL, &FALSE,
            "Enable the Statistics Module");

    options[i++] = register_option("LB_DROM", "--drom",
            OPT_BOOL_T, &opt_drom, RO, OPTIONAL, &FALSE,
            "Enable the Dynamic Resource Ownership Manager Module");

    // MPI
    options[i++] = register_option("LB_JUST_BARRIER", "--just-barrier",
            OPT_BOOL_T, &opt_just_barrier, RW, OPTIONAL, &FALSE,
            "When in MPI, lend resources only in MPI_Barrier calls");

    options[i++] = register_option("LB_LEND_MODE", "--lend-mode",
            OPT_BLCK_T, &opt_lend_mode, RW, OPTIONAL, "1CPU",
            "When in MPI, whether to keep at least <1CPU> (default), or lend all of them <BLOCK>");

    // verbose
    options[i++] = register_option("LB_VERBOSE", "--verbose",
            OPT_VB_T, &opt_verbose, RO, OPTIONAL, "",
            "Verbose components. It may contain the tags: api, microlb, shmem, mpi_api, "
            "mpi_intercept, stats, drom. They can appear in any order, delimited by the "
            "character :\n"
            "  e.g.:  LB_VERBOSE=\"api:shmem:drom\"");

    options[i++] = register_option("LB_VERBOSE_FORMAT", "--verbose_format",
            OPT_VBFMT_T, &opt_verbose_fmt, RO, OPTIONAL, "node:pid:thread",
            "Verbose format. It may contain the tags: node, pid, mpinode, mpirank, thread. "
            "They can appear in any order, delimited by the character :");

    // tracing
    options[i++] = register_option("LB_TRACE_ENABLED", "--trace-enabled",
            OPT_BOOL_T, &opt_trace_enabled, RO, OPTIONAL, &TRUE,
            "Enable tracing, default: true.");

    options[i++] = register_option("LB_TRACE_COUNTERS", "--trace-counters",
            OPT_BOOL_T, &opt_trace_counters, RW, OPTIONAL, &TRUE,
            "Enable counters tracing, default: true.");

    // misc
    options[i++] = register_option("LB_MASK", "--mask",
            OPT_STR_T, &opt_mask, RO, OPTIONAL, NULL,
            "Cpuset mask owned by DLB");

    options[i++] = register_option("LB_GREEDY", "--greedy",
            OPT_BOOL_T, &opt_greedy, RW, OPTIONAL, &FALSE,
            "Greedy option for LeWI policy");

    char default_key[8];
    snprintf(default_key, 8, "%d", getuid());
    options[i++] = register_option("LB_SHM_KEY", "--shm-key",
            OPT_STR_T, &opt_shm_key, RO, OPTIONAL, default_key,
            "Shared Memory key. It determines the namefile to which DLB processes can "
            "interconnect, default: user ID");

    options[i++] = register_option("LB_BIND", "--bind",
            OPT_BOOL_T, &opt_bind, RO, OPTIONAL, &FALSE,
            "Bind option for LeWI, currently disabled");

    options[i++] = register_option("LB_THREAD_DISTRIBUTION", "--thread-distribution",
            OPT_STR_T, &opt_thread_distribution, RO, OPTIONAL, NULL,
            "Thread distribution");

    options[i++] = register_option("LB_AGGRESSIVE_INIT", "--aggressive_init",
            OPT_BOOL_T, &opt_aggressive_init, RO, OPTIONAL, &FALSE,
            "Create as many threads as necessary during the process startup");

    options[i++] = register_option("LB_PRIORITIZE_LOCALITY", "--prioritize-locality",
            OPT_BOOL_T, &opt_prioritize_locality, RW, OPTIONAL, &FALSE,
            "Prioritize resource sharing by HW proximity");

    num_options = i;
    ensure(num_options<=MAX_OPTIONS, "Number of options registered greater than maximum" );
}

void options_finalize(void) {
    int i;
    for (i=0; i<num_options; ++i) {
        free(options[i]);
    }
    free(options);
    free(lb_args);
}

/* API Setter */
int options_set_variable(const char *var_name, const char *value) {
    option_t *option = NULL;
    int i;
    for (i=0; i<num_options; ++i) {
        if (strcasecmp(options[i]->var_name, var_name) == 0) {
            option = options[i];
            break;
        }
    }

    if (!option) return -1;
    if (option->readonly) return -1;
    if (!value) return -1;

    switch(option->type) {
        case OPT_BOOL_T:
            parse_bool(value, (bool*)option->value);
            break;
        case OPT_INT_T:
            parse_int(value, (int*)option->value);
            break;
        case OPT_STR_T:
            strncpy((char*)option->value, value, MAX_OPTION_LENGTH);
            break;
        case OPT_BLCK_T:
            parse_blocking_mode(value, (blocking_mode_t*)option->value);
            break;
        case OPT_VB_T:
            parse_verbose_opts(value, (verbose_opts_t*)option->value);
            break;
        case OPT_VBFMT_T:
            parse_verbose_fmt(value, (verbose_fmt_t*)option->value);
            break;
    }

    return 0;
}

/* API Getter */
int options_get_variable(const char *var_name, char *value) {
    option_t *option = NULL;
    int i;
    for (i=0; i<num_options; ++i) {
        if (strcasecmp(options[i]->var_name, var_name) == 0) {
            option = options[i];
            break;
        }
    }

    if (!option) return -1;
    if (!value) return -1;

    switch(option->type) {
        case OPT_BOOL_T:
            sprintf(value, "%s", *(bool*)option->value ? "true" : "false");
            break;
        case OPT_INT_T:
            sprintf(value, "%d", *(int*)option->value);
            break;
        case OPT_STR_T:
            strncpy(value, (char*)option->value, MAX_OPTION_LENGTH);
            break;
        case OPT_BLCK_T:
            sprintf(value, "%s", *(blocking_mode_t*)option->value == ONE_CPU ? "1CPU" : "BLOCK");
            break;
        case OPT_VB_T:
            sprintf(value, "Type non-printable. Integer value: %d", *(int*)option->value);
            break;
        case OPT_VBFMT_T:
            sprintf(value, "Type non-printable. Integer value: %d", *(int*)option->value);
            break;
    }

    return 0;
}

/* API Printer */
void options_print_variables(void) {

    if (!options) return;

    char buffer[1024*16] = "DLB Options:\n";
    char *b = buffer + strlen(buffer);
    option_t *option = NULL;
    int i;
    for (i=0; i<num_options; ++i) {
        option = options[i];
        if (option) {
            b += sprintf(b, "%s, %s", option->var_name, option->arg_name);
            switch(option->type) {
                case OPT_BOOL_T:
                    b += sprintf(b, " (bool)");
                    break;
                case OPT_INT_T:
                    b += sprintf(b, " (int)");
                    break;
                default:
                    b += sprintf(b, " (string)");
            }
            if (option->readonly) {
                b += sprintf(b, " - (readonly)");
            }
            b += sprintf(b, "\n");
        }
    }
    info0(buffer);
}
