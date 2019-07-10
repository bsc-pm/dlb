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

#include "support/options.h"

#include "apis/dlb_types.h"
#include "apis/dlb_errors.h"
#include "support/types.h"
#include "support/mask_utils.h"
#include "support/debug.h"

#include <string.h>
#include <stddef.h>
#include <ctype.h>
#include <stdint.h>

typedef enum OptionFlags {
    OPT_CLEAR      = 0,
    OPT_READONLY   = 1 << 0,
    OPT_OPTIONAL   = 1 << 1,
    OPT_DEPRECATED = 1 << 2,
    OPT_ADVANCED   = 1 << 3,
    OPT_HIDDEN     = 1 << 4,
    OPT_UNUSED     = 1 << 5
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
    OPT_MPISET_T,   // mpi_set_t
    OPT_OMPTOPTS_T  // ompt_opts_t
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

/* Description is displaced 4 spaces */
#define OFFSET "    "
static const opts_dict_t options_dictionary[] = {
    // general options
    {
        .var_name       = "LB_POLICY",
        .arg_name       = "--policy",
        .default_value  = "no",
        .description    = "",
        .offset         = SIZE_MAX,
        .type           = OPT_POL_T,
        .flags          = OPT_READONLY | OPT_OPTIONAL | OPT_DEPRECATED | OPT_UNUSED
    }, {
        .var_name       = "LB_LEWI",
        .arg_name       = "--lewi",
        .default_value  = "no",
        .description    = OFFSET"Enable Lend When Idle. Processes using this mode can use LeWI\n"
                          OFFSET"API to lend and borrow resources.",
        .offset         = offsetof(options_t, lewi),
        .type           = OPT_BOOL_T,
        .flags          = OPT_READONLY | OPT_OPTIONAL
    }, {
        .var_name       = "LB_DROM",
        .arg_name       = "--drom",
        .default_value  = "no",
        .description    = OFFSET"Enable the Dynamic Resource Ownership Manager Module. Processes\n"
                          OFFSET"using this mode can receive requests from other processes to\n"
                          OFFSET"change its own process mask.",
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
        .description    = OFFSET"Enable the Shared Memory Barrier. Experimental mode. Processes\n"
                          OFFSET"can perform an intra-node barrier",
        .offset         = offsetof(options_t, barrier),
        .type           = OPT_BOOL_T,
        .flags          = OPT_READONLY | OPT_OPTIONAL | OPT_ADVANCED
    }, {
        .var_name       = "LB_NULL",
        .arg_name       = "--ompt",
        .default_value  = "no",
        .description    = OFFSET"Enable OpenMP performance tool. If running with an OMPT capable\n"
                          OFFSET"runtime, DLB can register itself as OpenMP Tool and perform\n"
                          OFFSET"some tasks, like thread pinning reallocation when the process\n"
                          OFFSET"mask changes or some LeWI features if enabled.",
        .offset         = offsetof(options_t, ompt),
        .type           = OPT_BOOL_T,
        .flags          = OPT_READONLY | OPT_OPTIONAL
    }, {
        .var_name       = "LB_MODE",
        .arg_name       = "--mode",
        .default_value  = "polling",
        .description    = OFFSET"Set interaction mode between DLB and the application. In polling\n"
                          OFFSET"mode, each process needs to poll DLB to acquire resources. In\n"
                          OFFSET"async mode, DLB creates a helper thread to call back the\n"
                          OFFSET"application when resources become available.",
        .offset         = offsetof(options_t, mode),
        .type           = OPT_MODE_T,
        .flags          = OPT_READONLY | OPT_OPTIONAL
    },
    // verbose
    {
        .var_name       = "LB_NULL",
        .arg_name       = "--quiet",
        .default_value  = "no",
        .description    = OFFSET"Suppress all output from DLB, even error messages.",
        .offset         = offsetof(options_t, quiet),
        .type           = OPT_BOOL_T,
        .flags          = OPT_READONLY | OPT_OPTIONAL | OPT_ADVANCED
    }, {
        .var_name       = "LB_VERBOSE",
        .arg_name       = "--verbose",
        .default_value  = "",
        .description    = OFFSET"Select which verbose components will be printed. Multiple\n"
                          OFFSET"components may be selected.",
        .offset         = offsetof(options_t, verbose),
        .type           = OPT_VB_T,
        .flags          = OPT_READONLY | OPT_OPTIONAL
    }, {
        .var_name       = "LB_VERBOSE_FORMAT",
        .arg_name       = "--verbose-format",
        .default_value  = "node:spid",
        .description    = OFFSET"Set the verbose format for the verbose messages. Multiple\n"
                          OFFSET"components may be selected but the order is predefined as\n"
                          OFFSET"shown in the possible values.",
        .offset         = offsetof(options_t, verbose_fmt),
        .type           = OPT_VBFMT_T,
        .flags          = OPT_READONLY | OPT_OPTIONAL
    },
    // instrument
    {
        .var_name       = "LB_NULL",
        .arg_name       = "--instrument",
        .default_value  = "yes",
        .description    = OFFSET"Enable Extrae instrumentation. This option requires the\n"
                          OFFSET"instrumented DLB library and the Extrae library. If both\n"
                          OFFSET"conditions are met, DLB will emit events such as the DLB\n"
                          OFFSET"calls, DLB modes, etc.",
        .offset         = offsetof(options_t, instrument),
        .type           = OPT_BOOL_T,
        .flags          = OPT_READONLY | OPT_OPTIONAL
    }, {
        .var_name       = "LB_NULL",
        .arg_name       = "--instrument-counters",
        .default_value  = "no",
        .description    = OFFSET"Enable counters on DLB events. If this option is enabled, DLB\n"
                          OFFSET"will emit events to Extrae with hardware counters information.\n"
                          OFFSET"This may significantly increase the size of the tracefile.",
        .offset         = offsetof(options_t, instrument_counters),
        .type           = OPT_BOOL_T,
        .flags          = OPT_READONLY | OPT_OPTIONAL
    },
    // LeWI
    {
        .var_name       = "LB_NULL",
        .arg_name       = "--lewi-mpi",
        .default_value  = "no",
        .description    = OFFSET"When in MPI, whether to lend the current CPU. If LeWI\n"
                          OFFSET"is enabled and DLB is intercepting MPI calls, DLB will\n"
                          OFFSET"lend the CPU that is being occupied while waiting fot the\n"
                          OFFSET"MPI call to finish.",
        .offset         = offsetof(options_t, lewi_mpi),
        .type           = OPT_BOOL_T,
        .flags          = OPT_OPTIONAL
    }, {
        .var_name       = "LB_NULL",
        .arg_name       = "--lewi-mpi-calls",
        .default_value  = "all",
        .description    = OFFSET"Select which type of MPI calls will make LeWI to lend their\n"
                          OFFSET"CPUs. If set to all, LeWI will act on all blocking MPI calls,\n"
                          OFFSET"If set to other values, only those types will trigger LeWI.",
        .offset         = offsetof(options_t, lewi_mpi_calls),
        .type           = OPT_MPISET_T,
        .flags          = OPT_OPTIONAL
    }, {
        .var_name       = "LB_NULL",
        .arg_name       = "--lewi-affinity",
        .default_value  = "nearby-first",
        .description    = OFFSET"Prioritize resource sharing based on hardware affinity.\n"
                          OFFSET"Nearby-first will try to assign first resources that share the\n"
                          OFFSET"same socket or NUMA node with the current process. Nearby-only\n"
                          OFFSET"will only assign those near the process. Spread-ifempty will\n"
                          OFFSET"prioritize also nearby resources, and then it will assign CPUS\n"
                          OFFSET"in other sockets or NUMA nodes only if there is no other\n"
                          OFFSET"that can benefit from those.",
        .offset         = offsetof(options_t, lewi_affinity),
        .type           = OPT_PRIO_T,
        .flags          = OPT_OPTIONAL
    }, {
        .var_name       = "LB_NULL",
        .arg_name       = "--lewi-ompt",
        .default_value  = "",
        .description    = OFFSET"OMPT policy flags for LeWI. If OMPT mode is enabled, set when\n"
                          OFFSET"DLB can automatically invoke LeWI functions to lend or borrow\n"
                          OFFSET"CPUs. If \"mpi\" is set, LeWI will be invoked before and after\n"
                          OFFSET"each eligible MPI call. If \"borrow\" is set, DLB will try to\n"
                          OFFSET"borrow CPUs before each non nested parallel construct. If the\n"
                          OFFSET"flag \"lend\" is set, DLB will lend all non used CPUs after\n"
                          OFFSET"each non nested parallel construct.\n"
                          OFFSET"Multiple flags can be selected at the same time.",
        .offset         = offsetof(options_t, lewi_ompt),
        .type           = OPT_OMPTOPTS_T,
        .flags          = OPT_OPTIONAL
    }, {
        .var_name       = "LB_NULL",
        .arg_name       = "--lewi-greedy",
        .default_value  = "no",
        .description    = OFFSET"Greedy option for LeWI policy.",
        .offset         = offsetof(options_t, lewi_greedy),
        .type           = OPT_BOOL_T,
        .flags          = OPT_OPTIONAL
    }, {
        .var_name       = "LB_NULL",
        .arg_name       = "--lewi-warmup",
        .default_value  = "no",
        .description    = OFFSET"Create as many threads as necessary during the process startup.",
        .offset         = offsetof(options_t, lewi_warmup),
        .type           = OPT_BOOL_T,
        .flags          = OPT_READONLY | OPT_OPTIONAL
    },
    // misc
    {
        .var_name       = "LB_SHM_KEY",
        .arg_name       = "--shm-key",
        .default_value  = "",
        .description    = OFFSET"Shared Memory key. By default, if key is empty, all processes\n"
                          OFFSET"will use a shared memory based on the user ID. If different\n"
                          OFFSET"processes start their execution with different keys, they will\n"
                          OFFSET"use different shared memories and they will not share resources.",
        .offset         = offsetof(options_t, shm_key),
        .type           = OPT_STR_T,
        .flags          = OPT_READONLY | OPT_OPTIONAL | OPT_ADVANCED
    }, {
        .var_name       = "LB_PREINIT_PID",
        .arg_name       = "--preinit-pid",
        .default_value  = "0",
        .description    = OFFSET"Process ID that pre-initializes the DLB process for DROM.",
        .offset         = offsetof(options_t, preinit_pid),
        .type           = OPT_INT_T,
        .flags          = OPT_READONLY | OPT_OPTIONAL | OPT_HIDDEN
    }, {
        .var_name       = "LB_DEBUG_OPTS",
        .arg_name       = "--debug-opts",
        .default_value  = "",
        .description    = OFFSET"Debug options.",
        .offset         = offsetof(options_t, debug_opts),
        .type           = OPT_DBG_T,
        .flags          = OPT_OPTIONAL | OPT_ADVANCED
    }
};
#undef OFFSET

enum { NUM_OPTIONS = sizeof(options_dictionary)/sizeof(opts_dict_t) };


static const opts_dict_t* get_entry_by_name(const char *name) {
    int i;
    for (i=0; i<NUM_OPTIONS; ++i) {
        const opts_dict_t *entry = &options_dictionary[i];
        if (strcasecmp(entry->var_name, name) == 0
                || strcasecmp(entry->arg_name, name) == 0) {
            return entry;
        }
    }
    return NULL;
}

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
        case(OPT_OMPTOPTS_T):
            return parse_ompt_opts(str_value, (ompt_opts_t*)option);
    }
    return DLB_ERR_NOENT;
}

static const char * get_value(option_type_t type, const void *option) {
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
        case OPT_OMPTOPTS_T:
            return ompt_opts_tostr(*(ompt_opts_t*)option);
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
    strcpy(dlb_args_copy, dlb_args);
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
        strcpy(dlb_args_from_api, dlb_args);
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
        strcpy(dlb_args_from_env, env);
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

        /* Warn if option is deprecated and has rhs */
        if (entry->flags & OPT_DEPRECATED && rhs) {
            warning("Option %s is deprecated, please see the documentation", entry->arg_name);
        }

        /* Skip iteration if option is not used anymore */
        if (entry->flags & OPT_UNUSED) {
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
            if (options->debug_opts & DBG_WERROR) {
                fatal("Unrecognized flags from DLB_Init: %s", str);
            } else {
                warning("Unrecognized flags from DLB_Init: %s", str);
            }
        }
        free(dlb_args_from_api);
    }
    if (dlb_args_from_env) {
        char *str = dlb_args_from_env;
        while(isspace((unsigned char)*str)) str++;
        if (strlen(str) > 0) {
            if (options->debug_opts & DBG_WERROR) {
                fatal("Unrecognized flags from DLB_ARGS: %s", str);
            } else {
                warning("Unrecognized flags from DLB_ARGS: %s", str);
            }
        }
        free(dlb_args_from_env);
    }
}

/* API Setter */
int options_set_variable(options_t *options, const char *var_name, const char *value) {
    int error;
    const opts_dict_t *entry = get_entry_by_name(var_name);
    if (entry) {
        if (entry->flags & OPT_READONLY) {
            error = DLB_ERR_PERM;
        } else {
            error = set_value(entry->type, (char*)options+entry->offset, value);
        }
    } else {
        error = DLB_ERR_NOENT;
    }

    return error;
}

/* API Getter */
int options_get_variable(const options_t *options, const char *var_name, char *value) {
    int error;
    const opts_dict_t *entry = get_entry_by_name(var_name);
    if (entry) {
        sprintf(value, "%s", get_value(entry->type, (char*)options+entry->offset));
        error = DLB_SUCCESS;
    } else {
        error = DLB_ERR_NOENT;
    }

    return error;
}

/* API Printer */
void options_print_variables(const options_t *options, bool print_extended) {
    enum { buffer_size = 8192 };
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

        /* Skip if hidden */
        if (entry->flags & OPT_HIDDEN) continue;

        /* Skip if advanced (unless print_extended) */
        if (entry->flags & OPT_ADVANCED
                && !print_extended) continue;

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
            case OPT_OMPTOPTS_T:
                b += sprintf(b, "[%s]", get_ompt_opts_choices());
                break;
            default:
                b += sprintf(b, "(unknown)");
        }
        b += sprintf(b, "\n");

        /* Description if print_extended */
        if (print_extended) {
            b += sprintf(b, "%s\n\n", entry->description);
        }

    }

    b += sprintf(b, "\n"
                    "Boolean options accept both standalone flags and 'yes'/'no' parameters.\n"
                    "These are equivalent flags:\n"
                    "    export DLB_ARGS=\"--lewi --no-instrument\"\n"
                    "    export DLB_ARGS=\"--lewi=yes --instrument=no\"\n");

    info0("%s", buffer);
}

void options_print_lewi_flags(const options_t *options) {
    const opts_dict_t *entry;

    // --lewi-mpi
    bool default_lewi_mpi;
    entry = get_entry_by_name("--lewi-mpi");
    parse_bool(entry->default_value, &default_lewi_mpi);

    // --lewi-mpi-calls
    mpi_set_t default_lewi_mpi_calls;
    entry = get_entry_by_name("--lewi-mpi-calls");
    parse_mpiset(entry->default_value, &default_lewi_mpi_calls);

    // --lewi-affinity
    priority_t default_lewi_affinity;
    entry = get_entry_by_name("--lewi-affinity");
    parse_priority(entry->default_value, &default_lewi_affinity);

    // --lewi-ompt
    ompt_opts_t default_lewi_ompt;
    entry = get_entry_by_name("--lewi-ompt");
    parse_ompt_opts(entry->default_value, &default_lewi_ompt);

    if (options->lewi_mpi != default_lewi_mpi
            || options->lewi_mpi_calls != default_lewi_mpi_calls
            || options->lewi_affinity != default_lewi_affinity
            || options->lewi_ompt != default_lewi_ompt) {
        info0("LeWI options:");
        if (options->lewi_mpi != default_lewi_mpi) {
            info0("  --lewi-mpi");
        }
        if (options->lewi_mpi_calls != default_lewi_mpi_calls) {
            info0("  --lewi-mpi-calls=%s", mpiset_tostr(options->lewi_mpi_calls));
        }
        if (options->lewi_affinity != default_lewi_affinity) {
            info0("  --lewi-affinity=%s", priority_tostr(options->lewi_affinity));
        }
        if (options->lewi_ompt != default_lewi_ompt) {
            info0("  --lewi-omp=%s", ompt_opts_tostr(options->lewi_ompt));
        }
    }
}
