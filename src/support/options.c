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

#include "support/options.h"

#include "apis/dlb_types.h"
#include "apis/dlb_errors.h"
#include "support/types.h"
#include "support/mask_utils.h"
#include "support/debug.h"

#include <string.h>
#include <stddef.h>

typedef enum OptionTypes {
    OPT_BOOL_T,
    OPT_INT_T,
    OPT_STR_T,
    OPT_BLCK_T,     // blocking_mode_t
    OPT_VB_T,       // verbose_opts_t
    OPT_VBFMT_T,    // verbose_fmt_t
    OPT_DBG_T,      // debug_opts_t
    OPT_PRIO_T,     // priority_t
    OPT_POL_T,      // policy_t
    OPT_MASK_T,     // cpu_set_t
    OPT_MODE_T      // interaction_mode_t
} option_type_t;

typedef struct {
    char          var_name[MAX_OPTION_LENGTH];    // LB_OPTION
    char          arg_name[MAX_OPTION_LENGTH];    // --option
    char          default_value[MAX_OPTION_LENGTH];
    char          description[MAX_DESCRIPTION];
    option_type_t type;
    size_t        offset;
    bool          readonly;
    bool          optional;
} opts_dict_t;


static const opts_dict_t options_dictionary[] = {
    // modules
    {
        .var_name       = "LB_POLICY",
        .arg_name       = "--policy",
        .default_value  = "no",
        .description    = "Lend Balancing Policy",
        .type           = OPT_POL_T,
        .offset         = offsetof(options_t, lb_policy),
        .readonly       = true,
        .optional       = true
    }, {
        .var_name       = "LB_STATISTICS",
        .arg_name       = "--statistics",
        .default_value  = "no",
        .description    = "Enable the Statistics Module",
        .type           = OPT_BOOL_T,
        .offset         = offsetof(options_t, statistics),
        .readonly       = true,
        .optional       = true
    }, {
        .var_name       = "LB_DROM",
        .arg_name       = "--drom",
        .default_value  = "no",
        .description    = "Enable the Dynamic Resource Ownership Manager Module",
        .type           = OPT_BOOL_T,
        .offset         = offsetof(options_t, drom),
        .readonly       = true,
        .optional       = true
    }, {
        .var_name       = "LB_BARRIER",
        .arg_name       = "--barrier",
        .default_value  = "no",
        .description    = "Enable the Shared Memory Barrier",
        .type           = OPT_BOOL_T,
        .offset         = offsetof(options_t, barrier),
        .readonly       = true,
        .optional       = true
    }, {
        .var_name       = "LB_MODE",
        .arg_name       = "--mode",
        .default_value  = "polling",
        .description    = "Select mode: polling / async",
        .type           = OPT_MODE_T,
        .offset         = offsetof(options_t, mode),
        .readonly       = true,
        .optional       = true
    },
    // MPI
    {
        .var_name       = "LB_JUST_BARRIER",
        .arg_name       = "--just-barrier",
        .default_value  = "no",
        .description    = "When in MPI, lend resources only in MPI_Barrier calls",
        .type           = OPT_BOOL_T,
        .offset         = offsetof(options_t, mpi_just_barrier),
        .readonly       = false,
        .optional       = true
    }, {
        .var_name       = "LB_LEND_MODE",
        .arg_name       = "--lend-mode",
        .default_value  = "1CPU",
        .description    = "When in MPI, whether to keep at least 1 CPU or lend all of them",
        .type           = OPT_BLCK_T,
        .offset         = offsetof(options_t, mpi_lend_mode),
        .readonly       = false,
        .optional       = true
    },
    // verbose
    {
        .var_name      = "LB_VERBOSE",
        .arg_name      = "--verbose",
        .default_value = "",
        .description   = "Verbose components",
        .type          = OPT_VB_T,
        .offset        = offsetof(options_t, verbose),
        .readonly      = true,
        .optional      = true
    }, {
        .var_name      = "LB_VERBOSE_FORMAT",
        .arg_name      = "--verbose-format",
        .default_value = "node:pid:thread",
        .description   = "Verbose format",
        .type          = OPT_VBFMT_T,
        .offset        = offsetof(options_t, verbose_fmt),
        .readonly      = true,
        .optional      = true
    },
    // tracing
    {
        .var_name      = "LB_TRACE_ENABLED",
        .arg_name      = "--trace-enabled",
        .default_value = "yes",
        .description   = "Enable tracing",
        .type          = OPT_BOOL_T,
        .offset        = offsetof(options_t, trace_enabled),
        .readonly      = true,
        .optional      = true
    }, {
        .var_name      = "LB_TRACE_COUNTERS",
        .arg_name      = "--trace-counters",
        .default_value = "no",
        .description   = "Enable counters on DLB events",
        .type          = OPT_BOOL_T,
        .offset        = offsetof(options_t, trace_counters),
        .readonly      = true,
        .optional      = true
    },
    // misc
    {
        .var_name      = "LB_PRIORITY",
        .arg_name      = "--priority",
        .default_value = "affinity_first",
        .description   = "Priorize resource sharing by HW affinity",
        .type          = OPT_PRIO_T,
        .offset        = offsetof(options_t, priority),
        .readonly      = false,
        .optional      = true
    }, {
        .var_name      = "LB_SHM_KEY",
        .arg_name      = "--shm-key",
        .default_value = "",
        .description   = "Shared Memory key. It determines the namefile to which "
            "DLB processes can interconnect, default: user ID",
        .type          = OPT_STR_T,
        .offset        = offsetof(options_t, shm_key),
        .readonly      = true,
        .optional      = true
    }, {
        .var_name      = "LB_GREEDY",
        .arg_name      = "--greedy",
        .default_value = "no",
        .description   = "Greedy option for LeWI policy",
        .type          = OPT_BOOL_T,
        .offset        = offsetof(options_t, greedy),
        .readonly      = false,
        .optional      = true
    }, {
        .var_name      = "LB_AGGRESSIVE_INIT",
        .arg_name      = "--aggressive-init", .default_value = "",
        .description   = "Create as many threads as necessary during the process startup, "
            "only for LeWI",
        .type          = OPT_BOOL_T,
        .offset        = offsetof(options_t, aggressive_init),
        .readonly      = true,
        .optional      = true
    }, {
        .var_name      = "LB_DEBUG_OPTS",
        .arg_name      = "--debug-opts",
        .default_value = "",
        .description   = "Debug options list: (register-signals, return-stolen). "
            "Delimited by the character :",
        .type          = OPT_DBG_T,
        .offset        = offsetof(options_t, debug_opts),
        .readonly      = false,
        .optional      = true
    }
};

enum { NUM_OPTIONS = sizeof(options_dictionary)/sizeof(opts_dict_t) };


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
        case(OPT_MASK_T):
            mu_parse_mask(str_value, (cpu_set_t*)option);
            break;
        case(OPT_MODE_T):
            parse_mode(str_value, (interaction_mode_t*)option);
            break;
    }
}

static const char * get_value(option_type_t type, void *option/*, char *str_value*/) {
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
        case OPT_MASK_T:
            return mu_to_str((cpu_set_t*)option);
        case OPT_MODE_T:
            return mode_tostr(*(interaction_mode_t*)option);
    }
    return "unknown";
}

/* Parse LB_ARGS and remove argument if found */
static void parse_lb_args(const char *lb_args, const char *arg_name, char* arg_value) {
    *arg_value = 0;
    // Tokenize a copy of lb_args with " "(blank) delimiter
    char *end_space = NULL;
    size_t len = strlen(lb_args) + 1;
    char *lb_args_copy = malloc(sizeof(char)*len);
    strncpy(lb_args_copy, lb_args, len);
    char *token = strtok_r(lb_args_copy, " ", &end_space);
    while (token) {
        // Break token into two tokens "--argument" = "value"
        char *end_equal;
        char *argument = strtok_r(token, "=", &end_equal);
        fatal_cond(!argument, "Bad format parsing LB_ARGS: --argument=value");

        if (strcmp(argument, arg_name) == 0) {
            // Get value to return
            char *value = strtok_r(NULL, "=", &end_equal);
            fatal_cond(!value, "Bad format parsing LB_ARGS: --argument=value");
            strncpy(arg_value, value, MAX_OPTION_LENGTH);

            // Remove "--argument=value" from lb_args
            char *dest = strstr(lb_args, argument);
            char *src = dest+strlen(argument)+1+strlen(value);
            size_t n = strlen(src) + 1;
            memmove(dest, src, n);

            // We do not break here, we keep looping to allow option overwriting
        }
        // next token
        token = strtok_r(NULL, " ", &end_space);
    }
    free(lb_args_copy);
}

/* Initialize options struct from either argument, or env. variable */
void options_init(options_t *options, const char *lb_args_from_api) {
    // Copy lb_args_from_api if present, or LB_ARGS env. variable otherwise
    char *lb_args = NULL;
    const char *lb_args_from_env = getenv("LB_ARGS");
    const char *lb_args_to_copy = (lb_args_from_api) ? lb_args_from_api : lb_args_from_env;
    if (lb_args_to_copy) {
        size_t len = strlen(lb_args_to_copy) + 1;
        lb_args = malloc(sizeof(char)*len);
        strncpy(lb_args, lb_args_to_copy, len);
    }

    int i;
    for (i=0; i<NUM_OPTIONS; ++i) {
        // For each entry in the dictionary, check the corresponding LB_OPT and --opt fields
        // and set the associated variable in the struct options
        const opts_dict_t *entry = &options_dictionary[i];

        // Obtain str_value from precedence: lb_args -> LB_var -> default
        const char *str_value = NULL;
        char *arg_value = NULL;
        if (lb_args) {
            arg_value = malloc(MAX_OPTION_LENGTH*sizeof(char));
            parse_lb_args(lb_args, entry->arg_name, arg_value);
            str_value = arg_value;
        }
        str_value = (str_value && strlen(str_value)>0) ? str_value : getenv(entry->var_name);
        if (!str_value) {
            fatal_cond(!entry->optional, "Variable %s must be defined", entry->var_name);
            str_value = entry->default_value;
        }

        set_value(entry->type, (char*)options+entry->offset, str_value);

        if (lb_args) {
            free(arg_value);
        }
    }

    if (lb_args) {
        free(lb_args);
    }
}

/* API Setter */
int options_set_variable(options_t *options, const char *var_name, const char *value) {
    int error = DLB_ERR_NOENT;

    // find the dictionary entry
    int i;
    for (i=0; i<NUM_OPTIONS; ++i) {
        const opts_dict_t *entry = &options_dictionary[i];
        if (strcasecmp(entry->var_name, var_name) == 0
                || strcasecmp(entry->arg_name, var_name) == 0) {
            if (entry->readonly) {
                error = DLB_ERR_PERM;
            } else {
                set_value(entry->type, (char*)options+entry->offset, value);
                error = DLB_SUCCESS;
            }
            break;
        }
    }

    return error;
}

/* API Getter */
int options_get_variable(const options_t *options, const char *var_name, char *value) {
    int error = DLB_ERR_NOENT;

    // find the dictionary entry
    int i;
    for (i=0; i<NUM_OPTIONS; ++i) {
        const opts_dict_t *entry = &options_dictionary[i];
        if (strcasecmp(entry->var_name, var_name) == 0
                || strcasecmp(entry->arg_name, var_name) == 0) {
            sprintf(value, "%s", get_value(entry->type, (char*)options+entry->offset));
            error = DLB_SUCCESS;
            break;
        }
    }

    return error;
}

/* API Printer */
void options_print_variables(const options_t *options) {
    enum { buffer_size = 1024 };
    char buffer[buffer_size] = "DLB Options:\n";
    char *b = buffer + strlen(buffer);
    int i;
    for (i=0; i<NUM_OPTIONS; ++i) {
        const opts_dict_t *entry = &options_dictionary[i];

        /* Name */
        size_t name_len = strlen(entry->arg_name) + 1;
        b += sprintf(b, "%s:%s", entry->arg_name, name_len<8?"\t\t\t":name_len<16?"\t\t":"\t");

        /* Value */
        const char *value = get_value(entry->type, (char*)options+entry->offset);
        size_t value_len = strlen(value) + 1;
        b += sprintf(b, "%s %s", value, value_len<8?"\t\t\t":name_len<16?"\t\t":"\t");

        /* Choices */
        switch(entry->type) {
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
            case OPT_MODE_T:
                b += sprintf(b, "[%s]", get_mode_choices());
                break;
            default:
                b += sprintf(b, "(unknown)");
        }
        b += sprintf(b, "\n");
    }
    fatal_cond(strlen(buffer) > buffer_size, "Variables buffer size needs to be increased");
    info0(buffer);
}
