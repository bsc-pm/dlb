/*********************************************************************************/
/*  Copyright 2009-2024 Barcelona Supercomputing Center                          */
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "support/options.h"

#include "apis/dlb_types.h"
#include "apis/dlb_errors.h"
#include "support/types.h"
#include "support/mask_utils.h"
#include "support/debug.h"
#include "LB_core/spd.h"

#include <string.h>
#include <stddef.h>
#include <ctype.h>
#include <stdint.h>
#include <limits.h>

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
    OPT_NEG_BOOL_T,
    OPT_INT_T,
    OPT_STR_T,
    OPT_PTR_PATH_T, // pointer to char[PATH_MAX]
    OPT_VB_T,       // verbose_opts_t
    OPT_VBFMT_T,    // verbose_fmt_t
    OPT_INST_T,     // instrument_items_t
    OPT_DBG_T,      // debug_opts_t
    OPT_LEWI_AFF_T, // lewi_affinity_t
    OPT_MASK_T,     // cpu_set_t
    OPT_MODE_T,     // interaction_mode_t
    OPT_MPISET_T,   // mpi_set_t
    OPT_OMPTOPTS_T, // omptool_opts_t
    OPT_TLPSUM_T,   // talp_summary_t
    OPT_TLPMOD_T,   // talp_model_t
    OPT_OMPTM_T     // omptm_version_t
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
        .var_name       = "LB_TALP",
        .arg_name       = "--talp",
        .default_value  = "no",
        .description    = OFFSET"Enable the TALP (Tracking Application Live Performance)\n"
                          OFFSET"module. Processes that enable this mode can obtain performance\n"
                          OFFSET"metrics at run time.",
        .offset         = offsetof(options_t, talp),
        .type           = OPT_BOOL_T,
        .flags          = OPT_OPTIONAL
    },{
        .var_name       = "LB_BARRIER",
        .arg_name       = "--barrier",
        .default_value  = "yes",
        .description    = OFFSET"Enable the Shared Memory Barrier. Processes can perform\n"
                          OFFSET"intra-node barriers.",
        .offset         = offsetof(options_t, barrier),
        .type           = OPT_BOOL_T,
        .flags          = OPT_READONLY | OPT_OPTIONAL
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
        .description    = OFFSET"Suppress VERBOSE and INFO messages from DLB, still shows\n"
                          OFFSET"WARNING and PANIC messages.",
        .offset         = offsetof(options_t, quiet),
        .type           = OPT_BOOL_T,
        .flags          = OPT_READONLY | OPT_OPTIONAL
    }, {
        .var_name       = "LB_NULL",
        .arg_name       = "--silent",
        .default_value  = "no",
        .description    = OFFSET"Suppress all output from DLB, even error messages.",
        .offset         = offsetof(options_t, silent),
        .type           = OPT_BOOL_T,
        .flags          = OPT_READONLY | OPT_OPTIONAL
    }, {
        .var_name       = "LB_VERBOSE",
        .arg_name       = "--verbose",
        .default_value  = "no",
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
        .default_value  = "all",
        .description    = OFFSET"Enable Extrae instrumentation. This option requires the\n"
                          OFFSET"instrumented DLB library and the Extrae library. If both\n"
                          OFFSET"conditions are met, DLB will emit events such as the DLB\n"
                          OFFSET"calls, DLB modes, etc.",
        .offset         = offsetof(options_t, instrument),
        .type           = OPT_INST_T,
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
    }, {
        .var_name       = "LB_NULL",
        .arg_name       = "--instrument-extrae-nthreads",
        .default_value  = "0",
        .description    = OFFSET"Invoke Extrae_change_num_threads with the provided parameter\n"
                          OFFSET"at the start of the execution to pre-allocate a buffer per\n"
                          OFFSET"thread.",
        .offset         = offsetof(options_t, instrument_extrae_nthreads),
        .type           = OPT_INT_T,
        .flags          = OPT_READONLY | OPT_OPTIONAL | OPT_ADVANCED
    },
    // LeWI
    {
        .var_name       = "LB_NULL",
        .arg_name       = "--lewi-keep-one-cpu",
        .default_value  = "no",
        .description    = OFFSET"Whether the CPU of the thread that encounters a blocking call\n"
                          OFFSET"(MPI blocking call or DLB_Barrier) is also lent in the LeWI policy.\n"
                          OFFSET"This flag replaces the option --lewi-mpi, although negated.",
        .offset         = offsetof(options_t, lewi_keep_cpu_on_blocking_call),
        .type           = OPT_BOOL_T,
        .flags          = OPT_OPTIONAL
    }, {
        .var_name       = "LB_NULL",
        .arg_name       = "--lewi-respect-cpuset",
        .default_value  = "yes",
        .description    = OFFSET"Whether to respect the set of CPUs registered in DLB to\n"
                          OFFSET"use with LeWI. If disabled, all unknown CPUs are available\n"
                          OFFSET"for any process to borrow.",
        .offset         = offsetof(options_t, lewi_respect_cpuset),
        .type           = OPT_BOOL_T,
        .flags          = OPT_OPTIONAL
    }, {
        .var_name       = "LB_NULL",
        .arg_name       = "--lewi-respect-mask",
        .default_value  = "yes",
        .description    = OFFSET"Deprecated in favor of --lewi-respect-cpuset.\n",
        .offset         = offsetof(options_t, lewi_respect_cpuset),
        .type           = OPT_BOOL_T,
        .flags          = OPT_OPTIONAL | OPT_DEPRECATED
    }, {
        /* This is a deprecated option that overlaps with --lewi-keep-one-cpu.
         * It must be defined afterwards so that the default value of the
         * previous option does not overwrite this one */
        .var_name       = "LB_NULL",
        .arg_name       = "--lewi-mpi",
        .default_value  = "no",
        .description    = OFFSET"This option is now deprecated, but its previous behavior is now\n"
                          OFFSET"enabled by default.\n"
                          OFFSET"If you want to lend a CPU when in a blocking call, you may safely\n"
                          OFFSET"remove this option.\n"
                          OFFSET"If you want to keep this CPU, use --lewi-keep-one-cpu instead.",
        .offset         = offsetof(options_t, lewi_keep_cpu_on_blocking_call),
        .type           = OPT_NEG_BOOL_T,
        .flags          = OPT_OPTIONAL | OPT_DEPRECATED
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
        .arg_name       = "--lewi-barrier",
        .default_value  = "yes",
        .description    = OFFSET"Select whether DLB_Barrier calls (unnamed barriers only) will\n"
                          OFFSET"activate LeWI and lend their CPUs. Named barriers can be\n"
                          OFFSET"configured individually in the source code, or using the\n"
                          OFFSET"--lewi-barrier-select.",
        .offset         = offsetof(options_t, lewi_barrier),
        .type           = OPT_BOOL_T,
        .flags          = OPT_OPTIONAL | OPT_READONLY
    }, {
        .var_name       = "LB_NULL",
        .arg_name       = "--lewi-barrier-select",
        .default_value  = "",
        .description    = OFFSET"Comma separated values of barrier names that will activate\n"
                          OFFSET"LeWI. Warning: by setting this option to any non-blank value,\n"
                          OFFSET"the option --lewi-barrier is ignored. Use 'default' to also\n"
                          OFFSET"control the default unnamed barrier.\n"
                          OFFSET"e.g.: --lewi-barrier-select=default,barrier3",
        .offset         = offsetof(options_t, lewi_barrier_select),
        .type           = OPT_STR_T,
        .flags          = OPT_OPTIONAL | OPT_READONLY | OPT_ADVANCED
    }, {
        .var_name       = "LB_NULL",
        .arg_name       = "--lewi-affinity",
        .default_value  = "auto",
        .description    = OFFSET"Select which affinity policy to use.\n"
                          OFFSET"With 'auto', DLB will infer the LeWI policy for either classic\n"
                          OFFSET"(no mask support) or LeWI_mask depending on a number of factors.\n"
                          OFFSET"To override the automatic detection, use either 'none' or 'mask'\n"
                          OFFSET"to select the respective policy.\n"
                          OFFSET"The tokens 'nearby-first', 'nearby-only', and 'spread-ifempty'\n"
                          OFFSET"also enforce mask support with extended policies.\n"
                          OFFSET"'nearby-first' is the default policy when LeWI has mask support\n"
                          OFFSET"and will instruct LeWI to assign resources that share the same\n"
                          OFFSET"socket or NUMA node with the current process first, then the\n"
                          OFFSET"rest.\n"
                          OFFSET"'nearby-only' will make LeWI assign only those resources that\n"
                          OFFSET"are near the process.\n"
                          OFFSET"'spread-ifempty' will also prioritise nearby resources, but the\n"
                          OFFSET"rest will only be considered if all CPUs in that socket or NUMA\n"
                          OFFSET"node has been lent to DLB.",
        .offset         = offsetof(options_t, lewi_affinity),
        .type           = OPT_LEWI_AFF_T,
        .flags          = OPT_OPTIONAL
    }, {
        .var_name       = "LB_NULL",
        .arg_name       = "--lewi-ompt",
        .default_value  = "borrow",
        .description    = OFFSET"OMPT option flags for LeWI. If OMPT mode is enabled, set when\n"
                          OFFSET"DLB can automatically invoke LeWI functions to lend or borrow\n"
                          OFFSET"CPUs. If 'none' is set, LeWI will not be invoked automatically.\n"
                          OFFSET"If 'borrow' is set, DLB will try to borrow CPUs in certain\n"
                          OFFSET"situations; typically, before non nested parallel constructs if\n"
                          OFFSET"the OMPT thread manager is omp5 and on each task creation and\n"
                          OFFSET"task switch in other thread managers. (This option is the default\n"
                          OFFSET"and should be enough in most of the cases). If the flag 'lend'\n"
                          OFFSET"is set, DLB will lend all non used CPUs after each non nested\n"
                          OFFSET"parallel construct and task completion on external threads.\n"
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
    }, {
        .var_name       = "LB_NULL",
        .arg_name       = "--lewi-max-parallelism",
        .default_value  = "0",
        .description    = OFFSET"Set the maximum level of parallelism for the LeWI algorithm.",
        .offset         = offsetof(options_t, lewi_max_parallelism),
        .type           = OPT_INT_T,
        .flags          = OPT_READONLY | OPT_OPTIONAL
    }, {
        .var_name       = "LB_NULL",
        .arg_name       = "--lewi-color",
        .default_value  = "0",
        .description    = OFFSET"Set the LeWI color of the process, allowing the creation of\n"
                          OFFSET"different disjoint subgroups for resource sharing. Processes\n"
                          OFFSET"will only share resources with other processes of the same color.",
        .offset         = offsetof(options_t, lewi_color),
        .type           = OPT_INT_T,
        .flags          = OPT_READONLY | OPT_OPTIONAL
    },
    // talp
    {
        .var_name       = "LB_NULL",
        .arg_name       = "--talp-openmp",
        .default_value  = "no",
        .description    = OFFSET"Select whether to measure OpenMP metrics. (Experimental)",
        .offset         = offsetof(options_t, talp_openmp),
        .type           = OPT_BOOL_T,
        .flags          = OPT_READONLY | OPT_OPTIONAL
    },
    {
        .var_name       = "LB_NULL",
        .arg_name       = "--talp-papi",
        .default_value  = "no",
        .description    = OFFSET"Select whether to collect PAPI counters.",
        .offset         = offsetof(options_t, talp_papi),
        .type           = OPT_BOOL_T,
        .flags          = OPT_READONLY | OPT_OPTIONAL
    },
    {
        .var_name       = "LB_TALP_SUMM",
        .arg_name       = "--talp-summary",
        .default_value  = "pop-metrics",
        .description    = OFFSET"List of summaries, separated by ':', to write at the end\n"
                          OFFSET"of the execution: 'pop-metrics', the default option, will\n"
                          OFFSET"print a short report if no '--talp-output-file' is specified.\n"
                          OFFSET"Otherwise, a more verbose file is generated containing all\n"
                          OFFSET"metrics collected by TALP. 'node' and 'process' show a \n"
                          OFFSET"summary for each node and process respectively.\n"
                          OFFSET"\n"
                          OFFSET"Deprecated options:\n"
                          OFFSET"'pop-raw' will be removed in the next release. The output \n"
                          OFFSET"will be available using the 'pop-metrics' summary.",
        .offset         = offsetof(options_t, talp_summary),
        .type           = OPT_TLPSUM_T,
        .flags          = OPT_READONLY | OPT_OPTIONAL
    },
    {
        .var_name       = "LB_NULL",
        .arg_name       = "--talp-output-file",
        .default_value  = "",
        .description    = OFFSET"Write TALP metrics to a file. If this option is not provided,\n"
                          OFFSET"the output is printed to stderr.\n"
                          OFFSET"Accepted formats: *.json, *.csv. Any other for plain text.\n"
                          OFFSET"\n"
                          OFFSET"Deprecated formats:\n"
                          OFFSET"The *.xml file ending is deprecated and will be removed in\n"
                          OFFSET"the next release.",
        .offset         = offsetof(options_t, talp_output_file),
        .type           = OPT_PTR_PATH_T,
        .flags          = OPT_READONLY | OPT_OPTIONAL
    },
    {
        /* In the future, consider using an interval update timer instead of a boolean */
        .var_name       = "LB_NULL",
        .arg_name       = "--talp-external-profiler",
        .default_value  = "no",
        .description    = OFFSET"Enable live metrics update to the shared memory. This flag\n"
                          OFFSET"is only needed if there is an external program monitoring\n"
                          OFFSET"the application.",
        .offset         = offsetof(options_t, talp_external_profiler),
        .type           = OPT_BOOL_T,
        .flags          = OPT_READONLY | OPT_OPTIONAL
    },
    {
        .var_name       = "LB_NULL",
        .arg_name       = "--talp-regions-per-proc",
        .default_value  = "100",
        .description    = OFFSET"Number of TALP regions per process to allocate in the shared\n"
                          OFFSET"memory.",
        .offset         = offsetof(options_t, talp_regions_per_proc),
        .type           = OPT_INT_T,
        .flags          = OPT_READONLY | OPT_OPTIONAL | OPT_ADVANCED
    },
    {
        .var_name       = "LB_NULL",
        .arg_name       = "--talp-region-select",
        .default_value  = "",
        .description    = OFFSET"Select TALP regions to enable. The option accepts the\n"
                          OFFSET"special values 'all', to enable all TALP regions, and 'none'\n"
                          OFFSET"to disable them all. An empty value is equivalent to 'all'.\n"
                          OFFSET"Additionally, a comma separated list of region names may be\n"
                          OFFSET"specified to enable only these regions. The implicit monitoring\n"
                          OFFSET"region may be specified with the special token 'application'.\n"
                          OFFSET"Note that names with spaces are not supported.\n"
                          OFFSET"e.g.: --talp-region-select=none\n"
                          OFFSET"      --talp-region-select=application,region3",
        .offset         = offsetof(options_t, talp_region_select),
        .type           = OPT_STR_T,
        .flags          = OPT_READONLY | OPT_OPTIONAL
    },
    {
        .var_name       = "LB_NULL",
        .arg_name       = "--talp-model",
        .default_value  = "hybrid-v2",
        .description    = OFFSET"For development use only.\n"
                          OFFSET"Select which version of POP metrics to compute.",
        .offset         = offsetof(options_t, talp_model),
        .type           = OPT_TLPMOD_T,
        .flags          = OPT_READONLY | OPT_OPTIONAL | OPT_ADVANCED
    },
    // barrier
    {
        .var_name       = "LB_NULL",
        .arg_name       = "--barrier-id",
        .default_value  = "0",
        .description    = OFFSET"Barrier ID. Use different barrier id numbers for different\n"
                          OFFSET"processes to avoid unwanted synchronization.",
        .offset         = offsetof(options_t, barrier_id),
        .type           = OPT_INT_T,
        .flags          = OPT_READONLY | OPT_OPTIONAL | OPT_ADVANCED
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
        .var_name       = "LB_NULL",
        .arg_name       = "--ompt-thread-manager",
        .default_value  = "none",
        .description    = OFFSET"OMPT Thread Manager version.",
        .offset         = offsetof(options_t, omptm_version),
        .type           = OPT_OMPTM_T,
        .flags          = OPT_OPTIONAL
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

static int set_ptr_path_value(void *option, const char *str_value) {
    int path_len = snprintf(NULL, 0, "%s", str_value);
    if (path_len > 0) {
        *(char**)option = malloc(sizeof(char)*(path_len+1));
        snprintf(*(char**)option, PATH_MAX, "%s", str_value);
    } else {
        *(char**)option = NULL;
    }
    return DLB_SUCCESS;
}

static int set_value(option_type_t type, void *option, const char *str_value) {
    switch(type) {
        case OPT_BOOL_T:
            return parse_bool(str_value, (bool*)option);
        case OPT_NEG_BOOL_T:
            return parse_negated_bool(str_value, (bool*)option);
        case OPT_INT_T:
            return parse_int(str_value, (int*)option);
        case OPT_STR_T:
            snprintf(option, MAX_OPTION_LENGTH, "%s", str_value);
            return DLB_SUCCESS;
        case OPT_PTR_PATH_T:
            return set_ptr_path_value(option, str_value);
        case OPT_VB_T:
            return parse_verbose_opts(str_value, (verbose_opts_t*)option);
        case OPT_VBFMT_T:
            return parse_verbose_fmt(str_value, (verbose_fmt_t*)option);
        case OPT_INST_T:
            return parse_instrument_items(str_value, (instrument_items_t*)option);
        case OPT_DBG_T:
            return parse_debug_opts(str_value, (debug_opts_t*)option);
        case OPT_LEWI_AFF_T:
            return parse_lewi_affinity(str_value, (lewi_affinity_t*)option);
        case OPT_MASK_T:
            mu_parse_mask(str_value, (cpu_set_t*)option);
            return DLB_SUCCESS;
        case OPT_MODE_T:
            return parse_mode(str_value, (interaction_mode_t*)option);
        case OPT_MPISET_T:
            return parse_mpiset(str_value, (mpi_set_t*)option);
        case OPT_OMPTOPTS_T:
            return parse_omptool_opts(str_value, (omptool_opts_t*)option);
        case OPT_TLPSUM_T:
            return parse_talp_summary(str_value, (talp_summary_t*)option);
        case OPT_TLPMOD_T:
            return parse_talp_model(str_value, (talp_model_t*)option);
        case OPT_OMPTM_T:
            return parse_omptm_version(str_value, (omptm_version_t*)option);
    }
    return DLB_ERR_NOENT;
}

static const char * get_value(option_type_t type, const void *option) {
    static char int_value[8];
    switch(type) {
        case OPT_BOOL_T:
            return *(bool*)option ? "yes" : "no";
        case OPT_NEG_BOOL_T:
            return *(bool*)option ? "no" : "yes";
        case OPT_INT_T:
            sprintf(int_value, "%d", *(int*)option);
            return int_value;
        case OPT_STR_T:
            return (char*)option;
        case OPT_PTR_PATH_T:
            return *(const char**)option ? *(const char**)option : "";
        case OPT_VB_T:
            return verbose_opts_tostr(*(verbose_opts_t*)option);
        case OPT_VBFMT_T:
            return verbose_fmt_tostr(*(verbose_fmt_t*)option);
        case OPT_INST_T:
            return instrument_items_tostr(*(instrument_items_t*)option);
        case OPT_DBG_T:
            return debug_opts_tostr(*(debug_opts_t*)option);
        case OPT_LEWI_AFF_T:
            return lewi_affinity_tostr(*(lewi_affinity_t*)option);
        case OPT_MASK_T:
            return mu_to_str((cpu_set_t*)option);
        case OPT_MODE_T:
            return mode_tostr(*(interaction_mode_t*)option);
        case OPT_MPISET_T:
            return mpiset_tostr(*(mpi_set_t*)option);
        case OPT_OMPTOPTS_T:
            return omptool_opts_tostr(*(omptool_opts_t*)option);
        case OPT_TLPSUM_T:
            return talp_summary_tostr(*(talp_summary_t*)option);
        case OPT_TLPMOD_T:
            return talp_model_tostr(*(talp_model_t*)option);
        case OPT_OMPTM_T:
            return omptm_version_tostr(*(omptm_version_t*)option);
    }
    return "unknown";
}

static bool values_are_equivalent(option_type_t type, const char *value1, const char *value2) {
    switch(type) {
        case OPT_BOOL_T:
            return equivalent_bool(value1, value2);
        case OPT_NEG_BOOL_T:
            return equivalent_negated_bool(value1, value2);
        case OPT_INT_T:
            return equivalent_int(value1, value2);
        case OPT_STR_T:
            return strcmp(value1, value2) == 0;
        case OPT_PTR_PATH_T:
            return *(const char**)value1 && *(const char**)value2
                && strcmp(*(const char**)value1, *(const char**)value2) == 0;
        case OPT_VB_T:
            return equivalent_verbose_opts(value1, value2);
        case OPT_VBFMT_T:
            return equivalent_verbose_fmt(value1, value2);
        case OPT_INST_T:
            return equivalent_instrument_items(value1, value2);
        case OPT_DBG_T:
            return equivalent_debug_opts(value1, value2);
        case OPT_LEWI_AFF_T:
            return equivalent_lewi_affinity(value1, value2);
        case OPT_MASK_T:
            return mu_equivalent_masks(value1, value2);
        case OPT_MODE_T:
            return equivalent_mode(value1, value2);
        case OPT_MPISET_T:
            return equivalent_mpiset(value1, value2);
        case OPT_OMPTOPTS_T:
            return equivalent_omptool_opts(value1, value2);
        case OPT_TLPSUM_T:
            return equivalent_talp_summary(value1, value2);
        case OPT_TLPMOD_T:
            return equivalent_talp_model(value1, value2);
        case OPT_OMPTM_T:
            return equivalent_omptm_version_opts(value1, value2);
    }
    return false;
}

static void copy_value(option_type_t type, void *dest, const char *src) {
    switch(type) {
        case OPT_BOOL_T:
            memcpy(dest, src, sizeof(bool));
            break;
        case OPT_NEG_BOOL_T:
            memcpy(dest, src, sizeof(bool));
            break;
        case OPT_INT_T:
            memcpy(dest, src, sizeof(int));
            break;
        case OPT_STR_T:
            memcpy(dest, src, sizeof(char)*MAX_OPTION_LENGTH);
            break;
        case OPT_PTR_PATH_T:
            fatal("copy_value of type OPT_PTR_PATH_T not supported");
            break;
        case OPT_VB_T:
            memcpy(dest, src, sizeof(verbose_opts_t));
            break;
        case OPT_VBFMT_T:
            memcpy(dest, src, sizeof(verbose_fmt_t));
            break;
        case OPT_INST_T:
            memcpy(dest, src, sizeof(instrument_items_t));
            break;
        case OPT_DBG_T:
            memcpy(dest, src, sizeof(debug_opts_t));
            break;
        case OPT_LEWI_AFF_T:
            memcpy(dest, src, sizeof(lewi_affinity_t));
            break;
        case OPT_MASK_T:
            memcpy(dest, src, sizeof(cpu_set_t));
            break;
        case OPT_MODE_T:
            memcpy(dest, src, sizeof(interaction_mode_t));
            break;
        case OPT_MPISET_T:
            memcpy(dest, src, sizeof(mpi_set_t));
            break;
        case OPT_OMPTOPTS_T:
            memcpy(dest, src, sizeof(omptool_opts_t));
            break;
        case OPT_TLPSUM_T:
            memcpy(dest, src, sizeof(talp_summary_t));
            break;
        case OPT_TLPMOD_T:
            memcpy(dest, src, sizeof(talp_model_t));
            break;
        case OPT_OMPTM_T:
            memcpy(dest, src, sizeof(omptm_version_t));
            break;
    }
}

/* Parse DLB_ARGS and remove argument if found */
static void parse_dlb_args(char *dlb_args, const char *arg_name, char* arg_value, size_t arg_max_len) {
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
                if (value) {
                    snprintf(arg_value, arg_max_len, "%s", value);
                } else {
                    warning("Bad format parsing of DLB_ARGS. Option %s with empty value", token);
                }
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

    /* Preallocate two buffers large enough to save any intermediate option value */
    char *arg_value_from_api = malloc(sizeof(char)*PATH_MAX);
    char *arg_value_from_env = malloc(sizeof(char)*PATH_MAX);

    int i;
    for (i=0; i<NUM_OPTIONS; ++i) {
        const opts_dict_t *entry = &options_dictionary[i];
        const char *rhs = NULL;                             /* pointer to rhs to be parsed */

        /* Set-up specific argument length */
        size_t arg_max_len;
        switch (entry->type) {
            case OPT_PTR_PATH_T:
                arg_max_len = PATH_MAX-1;
                break;
            default:
                arg_max_len = MAX_OPTION_LENGTH-1;
                break;
        }

        /* Reset intermediate buffers */
        arg_value_from_api[0] = '\0';
        arg_value_from_env[0] = '\0';

        /* Parse dlb_args from API */
        if (dlb_args_from_api) {
            parse_dlb_args(dlb_args_from_api, entry->arg_name, arg_value_from_api, arg_max_len);
            if (strlen(arg_value_from_api) > 0) {
                rhs = arg_value_from_api;
            }
        }

        /* Parse DLB_ARGS from env */
        if (dlb_args_from_env) {
            parse_dlb_args(dlb_args_from_env, entry->arg_name, arg_value_from_env, arg_max_len);
            if (strlen(arg_value_from_env) > 0) {
                if (rhs && !values_are_equivalent(entry->type, rhs, arg_value_from_env)) {
                    warning("Overwriting option %s = %s",
                            entry->arg_name, arg_value_from_env);
                }
                rhs = arg_value_from_env;
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
            warning("Option %s is deprecated:\n%s", entry->arg_name, entry->description);
        }

        /* Skip iteration if option is not used anymore */
        if (entry->flags & OPT_UNUSED) {
            continue;
        }

        /* Assign option = rhs, and nullify rhs if error */
        if (rhs) {
            int error = set_value(entry->type, (char*)options+entry->offset, rhs);
            if (error) {
                warning("Unrecognized %s value: %s. Setting default %s",
                        entry->arg_name, rhs, entry->default_value);
                rhs = NULL;
            }
        }

        /* Set default value if needed */
        if (!rhs && !(entry->flags & OPT_DEPRECATED)) {
            fatal_cond(!(entry->flags & OPT_OPTIONAL),
                    "Variable %s must be defined", entry->arg_name);
            int error = set_value(entry->type, (char*)options+entry->offset, entry->default_value);
            fatal_cond(error, "Internal error parsing default value %s=%s. Please, report bug at %s.",
                    entry->arg_name, entry->default_value, PACKAGE_BUGREPORT);
        }
    }

    /* Free intermediate buffers */
    free(arg_value_from_api);
    free(arg_value_from_env);

    /* Safety checks and free local buffers */
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

void options_finalize(options_t *options) {
    if (options->talp_output_file) {
        free(options->talp_output_file);
        options->talp_output_file = NULL;
    }
}

/* Obtain value of specific entry, either from DLB_ARGS or from thread_spd->options */
void options_parse_entry(const char *var_name, void *option) {
    const opts_dict_t *entry = get_entry_by_name(var_name);
    ensure(entry, "%s: bad variable name '%s'", __func__, var_name);

    if (thread_spd && thread_spd->dlb_initialized) {
        copy_value(entry->type, option, (char*)&thread_spd->options + entry->offset);
    } else {
        /* Simplified options_init just for this entry */

        /* Copy DLB_ARGS into a local buffer */
        char *dlb_args_from_env = NULL;
        const char *env = getenv("DLB_ARGS");
        if (env) {
            size_t len = strlen(env) + 1;
            dlb_args_from_env = malloc(sizeof(char)*len);
            strcpy(dlb_args_from_env, env);
        }

        /* Pointer to rhs to be parsed */
        const char *rhs = NULL;

        /* Set-up specific argument length */
        size_t arg_max_len;
        switch (entry->type) {
            case OPT_PTR_PATH_T:
                arg_max_len = PATH_MAX-1;
                break;
            default:
                arg_max_len = MAX_OPTION_LENGTH-1;
                break;
        }

        /* Preallocate a buffer large enough to save the option value */
        char *arg_value_from_env = malloc(sizeof(char)*(arg_max_len+1));
        arg_value_from_env[0] = '\0';

        /* Parse DLB_ARGS from env */
        if (dlb_args_from_env) {
            parse_dlb_args(dlb_args_from_env, entry->arg_name, arg_value_from_env, arg_max_len);
            if (strlen(arg_value_from_env) > 0) {
                rhs = arg_value_from_env;
            }
        }

        /* Assign option = rhs, and nullify rhs if error */
        if (rhs) {
            int error = set_value(entry->type, option, rhs);
            if (error) {
                rhs = NULL;
            }
        }

        /* Set default value if needed */
        if (!rhs) {
            set_value(entry->type, option, entry->default_value);
        }

        /* Free buffers */
        free(arg_value_from_env);
        if (dlb_args_from_env) {
            free(dlb_args_from_env);
        }
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

/* API Printer. Also, dlb -h/-hh output */
void options_print_variables(const options_t *options, bool print_extended) {

    /* Initialize buffer */
    print_buffer_t buffer;
    printbuffer_init(&buffer);

    /* Set up intermediate buffer per entry */
    enum { MAX_ENTRY_LEN = 256 };
    char entry_buffer[MAX_ENTRY_LEN];

    /* Header (last \n is not needed) */
    printbuffer_append(&buffer,
        "DLB Options:\n\n"
        "DLB is configured by setting these flags in the DLB_ARGS environment variable.\n"
        "Possible options are listed below:\n\n"
        "Option                  Current value   Possible value (type) / [choice] / {val1:val2}\n"
        "--------------------------------------------------------------------------------------"
        );

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

        /* Reset entry buffer */
        entry_buffer[0] = '\0';
        char *b = entry_buffer;
        size_t max_entry_len = MAX_ENTRY_LEN;

        /* Name */
        size_t name_len = strlen(entry->arg_name) + 1;
        if (name_len < 24) {
            /* Option + tabs until column 24 */
            b += snprintf(b, max_entry_len, "%s:%s",
                    entry->arg_name,
                    name_len < 8 ? "\t\t\t" : name_len < 16 ? "\t\t" : "\t");
        } else {
            /* Long option, break line */
            b += snprintf(b, max_entry_len, "%s:\n\t\t\t",
                    entry->arg_name);
        }

        /* Check if output was truncated, and update remaining entry length */
        fatal_cond((b - entry_buffer) >= MAX_ENTRY_LEN,
                "Output truncated in in %s. Please report bug at %s",
                __func__, PACKAGE_BUGREPORT);
        max_entry_len = MAX_ENTRY_LEN - (b - entry_buffer);

        /* Value */
        const char *value = get_value(entry->type, (char*)options+entry->offset);
        size_t value_len = strlen(value) + 1;
        b += snprintf(b, max_entry_len, "%s %s",
                value,
                value_len < 8 ? "\t\t" : value_len < 16 ? "\t" : "");

        /* Check if output was truncated, and update remaining entry length */
        fatal_cond((b - entry_buffer) >= MAX_ENTRY_LEN,
                "Output truncated in in %s. Please report bug at %s",
                __func__, PACKAGE_BUGREPORT);
        max_entry_len = MAX_ENTRY_LEN - (b - entry_buffer);

        /* Choices */
        switch(entry->type) {
            case OPT_BOOL_T:
                b += snprintf(b, max_entry_len, "(bool)");
                break;
            case OPT_NEG_BOOL_T:
                b += snprintf(b, max_entry_len, "(bool)");
                break;
            case OPT_INT_T:
                b += snprintf(b, max_entry_len, "(int)");
                break;
            case OPT_STR_T:
                b += snprintf(b, max_entry_len, "(string)");
                break;
            case OPT_PTR_PATH_T:
                b += snprintf(b, max_entry_len, "(path)");
                break;
            case OPT_VB_T:
                b += snprintf(b, max_entry_len, "{%s}", get_verbose_opts_choices());
                break;
            case OPT_VBFMT_T:
                b += snprintf(b, max_entry_len, "{%s}", get_verbose_fmt_choices());
                break;
            case OPT_INST_T:
                b += snprintf(b, max_entry_len, "{%s}", get_instrument_items_choices());
                break;
            case OPT_DBG_T:
                b += snprintf(b, max_entry_len, "{%s}", get_debug_opts_choices());
                break;
            case OPT_LEWI_AFF_T:
                b += snprintf(b, max_entry_len, "[%s]", get_lewi_affinity_choices());
                break;
            case OPT_MASK_T:
                b += snprintf(b, max_entry_len, "(cpuset)");
                break;
            case OPT_MODE_T:
                b += snprintf(b, max_entry_len, "[%s]", get_mode_choices());
                break;
            case OPT_MPISET_T:
                b += snprintf(b, max_entry_len, "[%s]", get_mpiset_choices());
                break;
            case OPT_OMPTOPTS_T:
                b += snprintf(b, max_entry_len, "[%s]", get_omptool_opts_choices());
                break;
            case OPT_TLPSUM_T:
                b += snprintf(b, max_entry_len, "{%s}", get_talp_summary_choices());
                break;
            case OPT_TLPMOD_T:
                b += snprintf(b, max_entry_len, "[%s]", get_talp_model_choices());
                break;
            case OPT_OMPTM_T:
                b += snprintf(b, max_entry_len, "[%s]", get_omptm_version_choices());
                break;
        }

        /* Check if output was truncated */
        fatal_cond((b - entry_buffer) >= MAX_ENTRY_LEN,
                "Buffer overflow in %s. Please report bug at %s",
                __func__, PACKAGE_BUGREPORT);

        /* Append entry listing */
        printbuffer_append(&buffer, entry_buffer);

        /* Append long description if print_extended */
        if (print_extended) {
            printbuffer_append(&buffer, entry->description);
            printbuffer_append(&buffer, "");
        }

    }

    /* Footer (last \n is not needed) */
    printbuffer_append(&buffer,
            "\n"
            "Boolean options accept both standalone flags and 'yes'/'no' parameters.\n"
            "These are equivalent flags:\n"
            "    export DLB_ARGS=\"--lewi --no-drom\"\n"
            "    export DLB_ARGS=\"--lewi=yes --drom=no\"");

    /* This function must print always and ignore options --quiet and --silent */
    info0_force_print("%s", buffer.addr);

    printbuffer_destroy(&buffer);
}

/* Print which LeWI flags are enabled during DLB_Init */
void options_print_lewi_flags(const options_t *options) {
    const opts_dict_t *entry;

    // --lewi-mpi
    bool default_lewi_keep_one_cpu;
    entry = get_entry_by_name("--lewi-keep-one-cpu");
    parse_bool(entry->default_value, &default_lewi_keep_one_cpu);

    // --lewi-mpi-calls
    mpi_set_t default_lewi_mpi_calls;
    entry = get_entry_by_name("--lewi-mpi-calls");
    parse_mpiset(entry->default_value, &default_lewi_mpi_calls);

    // --lewi-affinity
    lewi_affinity_t default_lewi_affinity;
    entry = get_entry_by_name("--lewi-affinity");
    parse_lewi_affinity(entry->default_value, &default_lewi_affinity);

    // --lewi-ompt
    omptool_opts_t default_lewi_ompt;
    entry = get_entry_by_name("--lewi-ompt");
    parse_omptool_opts(entry->default_value, &default_lewi_ompt);

    if (options->lewi_keep_cpu_on_blocking_call != default_lewi_keep_one_cpu
            || options->lewi_mpi_calls != default_lewi_mpi_calls
            || options->lewi_affinity != default_lewi_affinity
            || options->lewi_ompt != default_lewi_ompt) {
        info0("LeWI options:");
        if (options->lewi_keep_cpu_on_blocking_call != default_lewi_keep_one_cpu) {
            info0("  --lewi-keep-one-cpu");
        }
        if (options->lewi_mpi_calls != default_lewi_mpi_calls) {
            info0("  --lewi-mpi-calls=%s", mpiset_tostr(options->lewi_mpi_calls));
        }
        if (options->lewi_affinity != default_lewi_affinity) {
            info0("  --lewi-affinity=%s", lewi_affinity_tostr(options->lewi_affinity));
        }
        if (options->lewi_ompt != default_lewi_ompt) {
            info0("  --lewi-ompt=%s", omptool_opts_tostr(options->lewi_ompt));
        }
    }
}
