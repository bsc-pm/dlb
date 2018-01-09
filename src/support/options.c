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
#include <ctype.h>

typedef enum OptionFlags {
    OPT_CLEAR      = 0,
    OPT_READONLY   = 1 << 0,
    OPT_OPTIONAL   = 1 << 1,
    OPT_DEPRECATED = 1 << 2,
    OPT_ADVANCED   = 1 << 3
} option_flags_t;

typedef enum OptionTypes {
    OPT_BOOL_T,
    OPT_INT_T,
    OPT_STR_T,
    OPT_VB_T,       // verbose_opts_t
    OPT_VBFMT_T,    // verbose_fmt_t
    OPT_DBG_T,      // debug_opts_t
    OPT_PRIO_T,     // priority_t
    OPT_POL_T,      // policy_t
    OPT_MASK_T,     // cpu_set_t
    OPT_MODE_T,     // interaction_mode_t
    OPT_MPISET_T    // mpi_set_t
} option_type_t;

typedef struct {
    char           var_name[MAX_OPTION_LENGTH];    // LB_OPTION
    char           arg_name[MAX_OPTION_LENGTH];    // --option
    char           default_value[MAX_OPTION_LENGTH];
    char           description[MAX_DESCRIPTION];
    size_t         offset;
    option_type_t  type;
    option_flags_t flags;
} opts_dict_t;


static const opts_dict_t options_dictionary[] = {
    // general options
    {
        .var_name       = "LB_POLICY",
        .arg_name       = "--policy",
        .default_value  = "no",
        .description    = "",
        .offset         = 0,
        .type           = OPT_POL_T,
        .flags          = OPT_READONLY | OPT_OPTIONAL | OPT_DEPRECATED
    }, {
        .var_name       = "LB_LEWI",
        .arg_name       = "--lewi",
        .default_value  = "no",
        .description    = "Enable Lend When Idle. Processes using this mode can use"
                            " LeWI API to lend and borrow resources.",
        .offset         = offsetof(options_t, lewi),
        .type           = OPT_BOOL_T,
        .flags          = OPT_READONLY | OPT_OPTIONAL
    }, {
        .var_name       = "LB_DROM",
        .arg_name       = "--drom",
        .default_value  = "no",
        .description    = "Enable the Dynamic Resource Ownership Manager Module. Processes"
                            " using this mode can receive requests from other processes"
                            " to change its process mask.",
        .offset         = offsetof(options_t, drom),
        .type           = OPT_BOOL_T,
        .flags          = OPT_READONLY | OPT_OPTIONAL
    }, {
        /* to be replaced with --talp */
        .var_name       = "LB_STATISTICS",
        .arg_name       = "--statistics",
        .default_value  = "no",
        .description    = "",
        .offset         = offsetof(options_t, statistics),
        .type           = OPT_BOOL_T,
        .flags          = OPT_READONLY | OPT_OPTIONAL | OPT_DEPRECATED
    }, {
        .var_name       = "LB_BARRIER",
        .arg_name       = "--barrier",
        .default_value  = "no",
        .description    = "Enable the Shared Memory Barrier. Experimental intra-node barrier.",
        .offset         = offsetof(options_t, barrier),
        .type           = OPT_BOOL_T,
        .flags          = OPT_READONLY | OPT_OPTIONAL | OPT_ADVANCED
    }, {
        .var_name       = "LB_MODE",
        .arg_name       = "--mode",
        .default_value  = "polling",
        .description    = "Set interaction mode between DLB and the application or runtime.",
        .offset         = offsetof(options_t, mode),
        .type           = OPT_MODE_T,
        .flags          = OPT_READONLY | OPT_OPTIONAL
    },
    // verbose
    {
        .var_name       = "LB_VERBOSE",
        .arg_name       = "--verbose",
        .default_value  = "",
        .description    = "Set which verbose components will be shown. Only applicable to"
                            " debug library",
        .offset         = offsetof(options_t, verbose),
        .type           = OPT_VB_T,
        .flags          = OPT_READONLY | OPT_OPTIONAL
    }, {
        .var_name       = "LB_VERBOSE_FORMAT",
        .arg_name       = "--verbose-format",
        .default_value  = "node:pid:thread",
        .description    = "Set verbose format for the verbose messages.",
        .offset         = offsetof(options_t, verbose_fmt),
        .type           = OPT_VBFMT_T,
        .flags          = OPT_READONLY | OPT_OPTIONAL
    },
    // instrument
    {
        .var_name       = "LB_NULL",
        .arg_name       = "--instrument",
        .default_value  = "yes",
        .description    = "Enable Extrae instrumentation.",
        .offset         = offsetof(options_t, instrument),
        .type           = OPT_BOOL_T,
        .flags          = OPT_READONLY | OPT_OPTIONAL
    }, {
        .var_name       = "LB_NULL",
        .arg_name       = "--instrument-counters",
        .default_value  = "no",
        .description    = "Enable counters on DLB events.",
        .offset         = offsetof(options_t, instrument_counters),
        .type           = OPT_BOOL_T,
        .flags          = OPT_READONLY | OPT_OPTIONAL
    },
    // LeWI
    {
        .var_name       = "LB_NULL",
        .arg_name       = "--lewi-mpi",
        .default_value  = "no",
        .description    = "When in MPI, whether to lend the current CPU.",
        .offset         = offsetof(options_t, lewi_mpi),
        .type           = OPT_BOOL_T,
        .flags          = OPT_OPTIONAL
    }, {
        .var_name       = "LB_NULL",
        .arg_name       = "--lewi-mpi-calls",
        .default_value  = "all",
        .description    = "Set of MPI calls in which LeWI can lend its CPU.",
        .offset         = offsetof(options_t, lewi_mpi_calls),
        .type           = OPT_MPISET_T,
        .flags          = OPT_OPTIONAL
    }, {
        .var_name       = "LB_NULL",
        .arg_name       = "--lewi-affinity",
        .default_value  = "nearby-first",
        .description    = "Priorize resource sharing by HW affinity.",
        .offset         = offsetof(options_t, lewi_affinity),
        .type           = OPT_PRIO_T,
        .flags          = OPT_OPTIONAL
    }, {
        .var_name       = "LB_NULL",
        .arg_name       = "--lewi-greedy",
        .default_value  = "no",
        .description    = "Greedy option for LeWI policy.",
        .offset         = offsetof(options_t, lewi_greedy),
        .type           = OPT_BOOL_T,
        .flags          = OPT_OPTIONAL
    }, {
        .var_name       = "LB_NULL",
        .arg_name       = "--lewi-warmup",
        .default_value  = "no",
        .description    = "Create as many threads as necessary during the process startup.",
        .offset         = offsetof(options_t, lewi_warmup),
        .type           = OPT_BOOL_T,
        .flags          = OPT_READONLY | OPT_OPTIONAL
    },
    // misc
    {
        .var_name       = "LB_SHM_KEY",
        .arg_name       = "--shm-key",
        .default_value  = "",
        .description    = "Shared Memory key. It determines the namefile to which "
                            "DLB processes can interconnect.",
        .offset         = offsetof(options_t, shm_key),
        .type           = OPT_STR_T,
        .flags          = OPT_READONLY | OPT_OPTIONAL | OPT_ADVANCED
    }, {
        .var_name       = "LB_PREINIT_PID",
        .arg_name       = "--preinit-pid",
        .default_value  = "0",
        .description    = "Process ID that pre-initializes the DLB process",
        .offset         = offsetof(options_t, preinit_pid),
        .type           = OPT_INT_T,
        .flags          = OPT_READONLY | OPT_OPTIONAL | OPT_ADVANCED
    }, {
        .var_name       = "LB_DEBUG_OPTS",
        .arg_name       = "--debug-opts",
        .default_value  = "",
        .description    = "Debug options.",
        .offset         = offsetof(options_t, debug_opts),
        .type           = OPT_DBG_T,
        .flags          = OPT_OPTIONAL | OPT_ADVANCED
    }
};

enum { NUM_OPTIONS = sizeof(options_dictionary)/sizeof(opts_dict_t) };


static int set_value(option_type_t type, void *option, const char *str_value) {
    switch(type) {
        case(OPT_BOOL_T):
            return parse_bool(str_value, (bool*)option);
        case(OPT_INT_T):
            return parse_int(str_value, (int*)option);
        case(OPT_STR_T):
            strncpy(option, str_value, MAX_OPTION_LENGTH);
            return DLB_SUCCESS;
        case(OPT_VB_T):
            return parse_verbose_opts(str_value, (verbose_opts_t*)option);
        case(OPT_VBFMT_T):
            return parse_verbose_fmt(str_value, (verbose_fmt_t*)option);
        case(OPT_DBG_T):
            return parse_debug_opts(str_value, (debug_opts_t*)option);
        case(OPT_PRIO_T):
            return parse_priority(str_value, (priority_t*)option);
        case(OPT_POL_T):
            return parse_policy(str_value, (policy_t*)option);
        case(OPT_MASK_T):
            mu_parse_mask(str_value, (cpu_set_t*)option);
            return DLB_SUCCESS;
        case(OPT_MODE_T):
            return parse_mode(str_value, (interaction_mode_t*)option);
        case(OPT_MPISET_T):
            return parse_mpiset(str_value, (mpi_set_t*)option);
    }
    return DLB_ERR_NOENT;
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
        case OPT_MPISET_T:
            return mpiset_tostr(*(mpi_set_t*)option);
    }
    return "unknown";
}

/* Parse DLB_ARGS and remove argument if found */
static void parse_dlb_args(char *dlb_args, const char *arg_name, char* arg_value) {
    *arg_value = 0;
    /* Tokenize a copy of dlb_args with " "(blank) delimiter */
    char *progress = dlb_args;
    char *end_space = NULL;
    size_t len = strlen(dlb_args) + 1;
    char *dlb_args_copy = malloc(sizeof(char)*len);
    strncpy(dlb_args_copy, dlb_args, len);
    char *token = strtok_r(dlb_args_copy, " ", &end_space);
    while (token) {
        /* Each token is a complete string representing an option */

        bool remove_token = false;
        /* token length must be computed before tokenizing into arg=val */
        size_t token_len = strlen(token);
        /* progress pointer must be updated each iteration to skip spaces */
        while(isspace((unsigned char)*progress)) progress++;

        if (strchr(token, '=')) {
            /* Option is of the form --argument=value */
            char *end_equal;
            char *argument = strtok_r(token, "=", &end_equal);
            if (strcmp(argument, arg_name) == 0) {
                /* Obtain value */
                char *value = strtok_r(NULL, "=", &end_equal);
                fatal_cond(!value, "Bad format parsing DLB_ARGS: --argument=value");
                strncpy(arg_value, value, MAX_OPTION_LENGTH);
                remove_token = true;
            }
        } else {
            /* Option is of the form --argument/--no-argument */
            char *argument = token;
            if (strcmp(argument, arg_name) == 0) {
                /* Option value is 'yes' */
                strcpy(arg_value, "yes");
                remove_token = true;
            } else if (strncmp(argument, "--no-", 5) == 0
                    && strcmp(argument+5, arg_name+2) == 0) {
                /* Option value is 'no' */
                strcpy(arg_value, "no");
                remove_token = true;
            }
        }

        if (remove_token) {
            /* Remove token from dlb_args */
            char *dest = progress;
            char *src  = progress + token_len;
            size_t n = strlen(src) + 1;
            memmove(dest, src, n);
        } else {
            /* Token is not removed, update parsed progress pointer  */
            progress += token_len;
        }

        /* next token */
        token = strtok_r(NULL, " ", &end_space);
    }
    free(dlb_args_copy);
}

/* Initialize options struct from either argument, or env. variable */
void options_init(options_t *options, const char *dlb_args) {
    /* Copy dlb_args into a local buffer */
    char *dlb_args_from_api = NULL;
    if (dlb_args) {
        size_t len = strlen(dlb_args) + 1;
        dlb_args_from_api = malloc(sizeof(char)*len);
        strncpy(dlb_args_from_api, dlb_args, len);
    }

    /* Copy either DLB_ARGS or LB_ARGS into a local buffer */
    char *dlb_args_from_env = NULL;
    const char *env = getenv("DLB_ARGS");
    if (!env) {
        env = getenv("LB_ARGS");
        if (env) {
            warning("LB_ARGS is deprecated, please use DLB_ARGS");
        }
    }
    if (env) {
        size_t len = strlen(env) + 1;
        dlb_args_from_env = malloc(sizeof(char)*len);
        strncpy(dlb_args_from_env, env, len);
    }

    int i;
    for (i=0; i<NUM_OPTIONS; ++i) {
        const opts_dict_t *entry = &options_dictionary[i];
        const char *rhs = NULL;                             /* pointer to rhs to be parsed */
        char arg_value_from_api[MAX_OPTION_LENGTH] = "";    /* */
        char arg_value_from_env[MAX_OPTION_LENGTH] = "";    /* */

        /* Parse dlb_args from API */
        if (dlb_args_from_api) {
            parse_dlb_args(dlb_args_from_api, entry->arg_name, arg_value_from_api);
            if (strlen(arg_value_from_api) > 0) {
                rhs = arg_value_from_api;
            }
        }

        /* Parse DLB_ARGS from env */
        if (dlb_args_from_env) {
            parse_dlb_args(dlb_args_from_env, entry->arg_name, arg_value_from_env);
            if (strlen(arg_value_from_env) > 0) {
                if (rhs) {
                    warning("Ignoring option %s = %s due to DLB_Init precedence",
                            entry->arg_name, arg_value_from_env);
                } else {
                    rhs = arg_value_from_env;
                }
            }
        }

        /* Parse LB_option (to be deprecated soon) */
        const char *arg_value = getenv(entry->var_name);
        if (arg_value) {
            if (rhs) {
                warning("Ignoring option %s = %s due to DLB_ARGS precedence",
                        entry->var_name, arg_value);
            } else {
                warning("Option %s is to be deprecated in the near future, please use DLB_ARGS",
                        entry->var_name);
                rhs = arg_value;
            }
        }

        /* Warn if option is deprecated and has rhs, then skip iteration */
        if (entry->flags & OPT_DEPRECATED && rhs) {
            warning("Option %s is deprecated, please see the documentation", entry->arg_name);
            continue;
        }

        /* Assing option = rhs, and nullify rhs if error */
        if (rhs) {
            int error = set_value(entry->type, (char*)options+entry->offset, rhs);
            if (error) {
                warning("Unrecognized %s value: %s. Setting default %s",
                        entry->arg_name, rhs, entry->default_value);
                rhs = NULL;
            }
        }

        /* Set default value if needed */
        if (!rhs) {
            fatal_cond(!(entry->flags & OPT_OPTIONAL),
                    "Variable %s must be defined", entry->arg_name);
            int error = set_value(entry->type, (char*)options+entry->offset, entry->default_value);
            fatal_cond(error, "Bad parsing of default value %s", entry->default_value);
        }
    }

    /* Safety cheks and free local buffers */
    if (dlb_args_from_api) {
        char *str = dlb_args_from_api;
        while(isspace((unsigned char)*str)) str++;
        if (strlen(str) > 0) {
            warning("Unrecognized flags from DLB_Init: %s", str);
        }
        free(dlb_args_from_api);
    }
    if (dlb_args_from_env) {
        char *str = dlb_args_from_env;
        while(isspace((unsigned char)*str)) str++;
        if (strlen(str) > 0) {
            warning("Unrecognized flags from DLB_ARGS: %s", str);
        }
        free(dlb_args_from_env);
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
            if (entry->flags & OPT_READONLY) {
                error = DLB_ERR_PERM;
            } else {
                error = set_value(entry->type, (char*)options+entry->offset, value);
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
    enum { buffer_size = 2048 };
    char buffer[buffer_size] = "DLB Options:\n\n"
                                "The library configuration can be set using arguments\n"
                                "added to the DLB_ARGS environment variable.\n"
                                "Possible options are listed below:\n\n";
    char *b = buffer + strlen(buffer);
    int i;
    for (i=0; i<NUM_OPTIONS; ++i) {
        const opts_dict_t *entry = &options_dictionary[i];

        /* Skip if deprecated */
        if (entry->flags & OPT_DEPRECATED) continue;

        /* Skip if advanced */
        if (entry->flags & OPT_ADVANCED) continue;

        /* Name */
        size_t name_len = strlen(entry->arg_name) + 1;
        b += sprintf(b, "%s:%s", entry->arg_name, name_len<8?"\t\t\t":name_len<16?"\t\t":"\t");

        /* Value */
        const char *value = get_value(entry->type, (char*)options+entry->offset);
        size_t value_len = strlen(value) + 1;
        b += sprintf(b, "%s %s", value, value_len<8?"\t\t":value_len<16?"\t":"");

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
            case OPT_MPISET_T:
                b += sprintf(b, "[%s]", get_mpiset_choices());
                break;
            default:
                b += sprintf(b, "(unknown)");
        }
        b += sprintf(b, "\n");
    }

    b += sprintf(b, "\n"
                    "Boolean options accept both standalone flags and 'yes'/'no' parameters.\n"
                    "These are equivalent flags:\n"
                    "    export DLB_ARGS=\"--lewi --no-instrument\"\n"
                    "    export DLB_ARGS=\"--lewi=yes --instrument=no\"\n");

    fatal_cond(strlen(buffer) > buffer_size, "Variables buffer size needs to be increased");
    info0(buffer);
}

/* API Printer extra */
void options_print_variables_extra(const options_t *options) {
    enum { buffer_size = 2048 };
    char buffer[buffer_size] = "DLB_ARGS options:\n";
    char *b = buffer + strlen(buffer);
    int i;
    for (i=0; i<NUM_OPTIONS; ++i) {
        const opts_dict_t *entry = &options_dictionary[i];

        /* Skip if deprecated */
        if (entry->flags & OPT_DEPRECATED) continue;

        /* Name */
        b += sprintf(b, "  %s = ", entry->arg_name);

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
            case OPT_MPISET_T:
                b += sprintf(b, "[%s]", get_mpiset_choices());
                break;
            default:
                b += sprintf(b, "(unknown)");
        }

        /* Description */
        b += sprintf(b, "\n"
                        "  %s\n\n", entry->description);
    }

    fatal_cond(strlen(buffer) > buffer_size, "Variables buffer size needs to be increased");
    info0(buffer);
}
