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
#include "support/options.h"

#define MAX_OPTIONS 32
#define MAX_OPTION_LENGTH 32
#define MAX_DESCRIPTION 1024


// Options storage and internal getters
static policy_t opt_policy;
policy_t options_get_policy(void) { return opt_policy; }

static bool opt_statistics;
bool options_get_statistics(void) { return opt_statistics; }

static bool opt_drom;
bool options_get_drom(void) { return opt_drom; }

static bool opt_barrier;
bool options_get_barrier(void) { return opt_barrier; }

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

static bool opt_aggressive_init;
bool options_get_aggressive_init(void) { return opt_aggressive_init; }

static priority_t opt_priority;
priority_t options_get_priority(void) { return opt_priority; }

static debug_opts_t opt_debug_opts;
verbose_fmt_t options_get_debug_opts(void) { return opt_debug_opts; }


typedef enum OptionTypes {
    OPT_BOOL_T,
    OPT_INT_T,
    OPT_STR_T,
    OPT_BLCK_T,     // blocking_mode_t
    OPT_VB_T,       // verbose_opts_t
    OPT_VBFMT_T,    // verbose_fmt_t
    OPT_DBG_T,      // debug_opts_t
    OPT_PRIO_T,     // priority_t
    OPT_POL_T       // policy_t
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
static char *dlb_args = NULL;


static void set_value(option_type_t type, void *option, const char *str_value) {
    switch(type) {
        case(OPT_BOOL_T):
            parse_bool(str_value, (bool*)option);
            break;
        case(OPT_INT_T):
            parse_int(str_value, (int*)option);
            break;
        case(OPT_STR_T):
            strncpy(option, str_value, MAX_OPTION_LENGTH);
            break;
        case(OPT_BLCK_T):
            parse_blocking_mode(str_value, (blocking_mode_t*)option);
            break;
        case(OPT_VB_T):
            parse_verbose_opts(str_value, (verbose_opts_t*)option);
            break;
        case(OPT_VBFMT_T):
            parse_verbose_fmt(str_value, (verbose_fmt_t*)option);
            break;
        case(OPT_DBG_T):
            parse_debug_opts(str_value, (debug_opts_t*)option);
            break;
        case(OPT_PRIO_T):
            parse_priority(str_value, (priority_t*)option);
            break;
        case(OPT_POL_T):
            parse_policy(str_value, (policy_t*)option);
            break;
    }
}

static const char * get_value(option_type_t type, void *option) {
    static char int_value[8];
    switch(type) {
        case OPT_BOOL_T:
            return *(bool*)option ? "yes" : "no";
        case OPT_INT_T:
            sprintf(int_value, "%d", *(int*)option);
            return int_value;
        case OPT_STR_T:
            return (char*)option;
        case OPT_BLCK_T:
            return blocking_mode_tostr(*(blocking_mode_t*)option);
        case OPT_VB_T:
            return verbose_opts_tostr(*(verbose_opts_t*)option);
        case OPT_VBFMT_T:
            return verbose_fmt_tostr(*(verbose_fmt_t*)option);
        case OPT_DBG_T:
            return debug_opts_tostr(*(debug_opts_t*)option);
        case OPT_PRIO_T:
            return priority_tostr(*(priority_t*)option);
        case OPT_POL_T:
            return policy_tostr(*(policy_t*)option);
    }
    return "unknown";
}

/* Parse DLB_ARGS and remove argument if found */
static char* parse_env_arg(const char *arg_name) {
    static char value[MAX_OPTION_LENGTH];
    value[0] = 0;
    if (dlb_args && arg_name) {
        // Tokenize a copy of dlb_args with " " delimiter
        char *end_space;
        char *dlb_args_copy = malloc(1+sizeof(char)*strlen(dlb_args));
        strncpy(dlb_args_copy, dlb_args, strlen(dlb_args));
        char *token = strtok_r(dlb_args_copy, " ", &end_space);
        while (token) {
            // Break token into two tokens "--argument" = "value"
            char *end_equal;
            char *current_arg = strtok_r(token, "=", &end_equal);
            fatal_cond(!current_arg, "Bad format parsing DLB_ARGS: --argument=value");

            if (strcmp(current_arg, arg_name) == 0) {
                // Get value to return
                char *val = strtok_r(NULL, "=", &end_equal);
                fatal_cond(!val, "Bad format parsing DLB_ARGS: --argument=value");
                strncpy(value, val, MAX_OPTION_LENGTH);

                // Remove "token=value" from dlb_args
                char *dest = strstr(dlb_args, token);
                char *src = dest+strlen(token)+1+strlen(value);
                size_t n = 1+strlen(dest+strlen(token)+1+strlen(value));
                memmove(dest, src, n);

                break;
            }
            // next token
            token = strtok_r(NULL, " ", &end_space);
        }
        free(dlb_args_copy);
    }
    return (value[0]==0) ? NULL : value;
}

static option_t* register_option(const char *var, const char *arg, option_type_t type,
                                void *value, bool readonly, bool optional,
                                const char *default_value, const char *desc ) {

    option_t *new_option = malloc(sizeof(option_t));
    strncpy(new_option->var_name, var, MAX_OPTION_LENGTH);
    strncpy(new_option->arg_name, arg, MAX_OPTION_LENGTH);
    strncpy(new_option->description, desc, MAX_DESCRIPTION);
    new_option->type = type;
    new_option->value = value;
    new_option->readonly = readonly;
    new_option->optional = optional;

    // Obtain str_value from precedence: dlb_args -> LB_var -> default
    char *user_value = parse_env_arg(arg);
    if (!user_value) user_value = getenv(var);
    const char *str_value = (user_value && strlen(user_value)>0) ? user_value : default_value;

    set_value(type, (char*)new_option->value, str_value);

    fatal_cond(!optional && !new_option->value,
            "Variable %s must be defined", new_option->var_name);

    ++num_options;
    return new_option;
}

void options_init(void) {
    // options_init is not thread-safe
    if (options) return;

    const bool RO = true;
    const bool RW = false;
    const bool OPTIONAL = true;

    // Copy DLB_ARGS env. variable
    char *env_dlb_args = getenv("DLB_ARGS");
    if (env_dlb_args) {
        size_t len = strlen(env_dlb_args) + 1;
        dlb_args = malloc(sizeof(char)*len);
        strncpy(dlb_args, env_dlb_args, len);
    }

    // Fill options array
    options = malloc(sizeof(option_t*)*MAX_OPTIONS);

    // modules
    int i = 0;
    options[i++] = register_option("LB_POLICY", "--policy",
            OPT_POL_T, &opt_policy, RO, OPTIONAL, "no",
            "Lend Balancing Policy. Choose one of: \n"
            " - No: No policy\n"
            " - LeWI: Lend When Idle, basic policy\n"
            " - LeWI_mask: LeWI with mask support\n"
            " - auto_LeWI_mask: autonomous LeWI. To be used with highly malleable runtimes\n"
            " - RaL:");

    options[i++] = register_option("LB_STATISTICS", "--statistics",
            OPT_BOOL_T, &opt_statistics, RO, OPTIONAL, "no",
            "Enable the Statistics Module");

    options[i++] = register_option("LB_DROM", "--drom",
            OPT_BOOL_T, &opt_drom, RO, OPTIONAL, "no",
            "Enable the Dynamic Resource Ownership Manager Module");

    options[i++] = register_option("LB_BARRIER", "--barrier",
            OPT_BOOL_T, &opt_barrier, RO, OPTIONAL, "no",
            "Enable the Shared Memory Barrier");

    // MPI
    options[i++] = register_option("LB_JUST_BARRIER", "--just-barrier",
            OPT_BOOL_T, &opt_just_barrier, RW, OPTIONAL, "no",
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

    options[i++] = register_option("LB_VERBOSE_FORMAT", "--verbose-format",
            OPT_VBFMT_T, &opt_verbose_fmt, RO, OPTIONAL, "node:pid:thread",
            "Verbose format. It may contain the tags: node, pid, mpinode, mpirank, thread. "
            "They can appear in any order, delimited by the character :");

    // tracing
    options[i++] = register_option("LB_TRACE_ENABLED", "--trace-enabled",
            OPT_BOOL_T, &opt_trace_enabled, RO, OPTIONAL, "yes",
            "Enable tracing, default: true.");

    options[i++] = register_option("LB_TRACE_COUNTERS", "--trace-counters",
            OPT_BOOL_T, &opt_trace_counters, RW, OPTIONAL, "yes",
            "Enable counters tracing, default: true.");

    // misc
    options[i++] = register_option("LB_MASK", "--mask",
            OPT_STR_T, &opt_mask, RO, OPTIONAL, "",
            "Cpuset mask owned by DLB");

    options[i++] = register_option("LB_GREEDY", "--greedy",
            OPT_BOOL_T, &opt_greedy, RW, OPTIONAL, "no",
            "Greedy option for LeWI policy");

    char default_key[8];
    snprintf(default_key, 8, "%d", getuid());
    options[i++] = register_option("LB_SHM_KEY", "--shm-key",
            OPT_STR_T, &opt_shm_key, RO, OPTIONAL, default_key,
            "Shared Memory key. It determines the namefile to which DLB processes can "
            "interconnect, default: user ID");

    options[i++] = register_option("LB_BIND", "--bind",
            OPT_BOOL_T, &opt_bind, RO, OPTIONAL, "no",
            "Bind option for LeWI, currently disabled");

    options[i++] = register_option("LB_AGGRESSIVE_INIT", "--aggressive-init",
            OPT_BOOL_T, &opt_aggressive_init, RO, OPTIONAL, "no",
            "Create as many threads as necessary during the process startup");

    options[i++] = register_option("LB_PRIORITY", "--priority",
            OPT_PRIO_T, &opt_priority, RW, OPTIONAL, "affinity_first" ,
            "Priorize resource sharing by HW affinity");

    options[i++] = register_option("LB_DEBUG_OPTS", "--debug-opts",
            OPT_DBG_T, &opt_debug_opts, RW, OPTIONAL, "",
            "Debug options list: (register-signals, return-stolen). Delimited by the character :");

    ensure(num_options==i, "Number of registered options does not match");
    ensure(num_options<=MAX_OPTIONS, "Number of options registered greater than maximum" );
}

void options_finalize(void) {
    int i;
    if (options) {
        for (i=0; i<num_options; ++i) {
            free(options[i]);
        }
        free(options);
        free(dlb_args);
        options = NULL;
        dlb_args = NULL;
        num_options = 0;
    }
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

    set_value(option->type, (char*)option->value, value);
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

    sprintf(value, "%s", get_value(option->type, (char*)option->value));
    return 0;
}

/* API Printer */
void options_print_variables(void) {

    if (!options) return;

    enum { buffer_size = 1024 };
    char buffer[buffer_size] = "DLB Options:\n";
    char *b = buffer + strlen(buffer);
    option_t *option = NULL;
    int i;
    for (i=0; i<num_options; ++i) {
        option = options[i];
        if (option) {
            /* Name */
            size_t name_len = strlen(option->var_name) + 1;
            b += sprintf(b, "%s:%s", option->var_name, name_len<8?"\t\t\t":name_len<16?"\t\t":"\t");

            /* Value */
            const char *value = get_value(option->type, (char*)option->value);
            size_t value_len = strlen(value) + 1;
            b += sprintf(b, "%s %s", value, value_len<8?"\t\t\t":name_len<16?"\t\t":"\t");

            /* Choices */
            switch(option->type) {
                case OPT_BOOL_T:
                    b += sprintf(b, "(bool)");
                    break;
                case OPT_INT_T:
                    b += sprintf(b, "(int)");
                    break;
                case OPT_STR_T:
                    b += sprintf(b, "(string)");
                    break;
                case OPT_BLCK_T:
                    b += sprintf(b, "[%s]", get_blocking_mode_choices());
                    break;
                case OPT_VB_T:
                    b += sprintf(b, "{%s}", get_verbose_opts_choices());
                    break;
                case OPT_VBFMT_T:
                    b += sprintf(b, "{%s}", get_verbose_fmt_choices());
                    break;
                case OPT_DBG_T:
                    b += sprintf(b, "{%s}", get_debug_opts_choices());
                    break;
                case OPT_PRIO_T:
                    b += sprintf(b, "[%s]", get_priority_choices());
                    break;
                case OPT_POL_T:
                    b += sprintf(b, "[%s]", get_policy_choices());
                    break;
                default:
                    b += sprintf(b, "(unknown)");
            }
            b += sprintf(b, "\n");
        }
    }
    fatal_cond(strlen(buffer) > buffer_size, "Variables buffer size needs to be increased");
    info0(buffer);
}
