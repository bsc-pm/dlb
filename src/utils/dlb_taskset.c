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

/*! \page dlb_taskset Manage CPU affinity of DLB applications.
 *  \section synopsis SYNOPSIS
 *      <B>dlb_taskset</B> --list[=\underline{num_columns}] [--pid \underline{pid}]
 *    \n<B>dlb_taskset</B> --set \underline{cpu_list} --pid \underline{pid}
 *    \n<B>dlb_taskset</B> --set \underline{cpu_list} [--borrow] \underline{application}
 *    \n<B>dlb_taskset</B> --remove \underline{cpu_list} [--pid \underline{pid}]
 *    \n<B>dlb_taskset</B> --getpid \underline{id}
 *  \section description DESCRIPTION
 *      The command <B>dlb_taskset</B> can manage the CPU affinity of any DLB
 *      running process or new applications. The command can list the existing
 *      processes or modify the current CPU affinity.
 *
 *      Note that the utility does not only changes the CPU pinning of each
 *      thread but also forces the process to communicate with the underlying
 *      programming model runtime to modify the number of running threads.
 *
 *      Possible operations:
 *      <DL>
 *          <DT>-l, --list[=\underline{num_columns}]</DT>
 *          <DD>List the CPU affinity of all DLB processes as well as other
 *          information about its mask status. The operation accepts an
 *          optional parameter \underline{num_columns} to specify the maximum number
 *          of columns to be shown in the output, by default this number
 *          depends on the screen width. If <B>--pid</B> \underline{pid} is
 *          provided, show only the current affinity mask of that process.</DD>
 *
 *          <DT>-s, --set, -c, --cpus \underline{cpu_list}</DT>
 *          <DD>Set a new CPU affinity mask as specified in \underline{cpu_list} for
 *          an existing DLB process or a new one. This operation needs either a
 *          --pid \underline{pid} or an \underline{application}. If some CPU in the list
 *          is previously owned by other DLB process, remove first. See
 *          \ref format "CPU_LIST FORMAT".</DD>
 *
 *          <DT>-r, --remove \underline{cpu_list}</DT>
 *          <DD>Remove all CPUs specified in \underline{cpu_list} from any DLB
 *          process that currently owns that CPU. If a --pid \underline{pid} is
 *          provided, only act on that process. See \ref format "CPU_LIST FORMAT".
 *          </DD>
 *
 *          <DT>-g, --getpid \underline{id}</DT>
 *          <DD>Obtain the system Process ID of a given DLB internal \underline{id}.
 *          Note that it is not possible to obtain the DLB internal ID of a
 *          process so the usage of this option is discouraged, although it is
 *          kept for internal usage.</DD>
 *      </DL>
 *  \section options OPTIONS
 *      <DL>
 *          <DT>-p, --pid \underline{pid}</DT>
 *          <DD>Act only on the given \underline{pid}.</DD>
 *
 *          <DT>-b, --borrow</DT>
 *          <DD>Only valid when <B>dlb_taskset</B> launches a new application.
 *          This flag tells DLB to borrow CPUs instead of stealing them. This
 *          means that if any CPU was removed from any other process due to
 *          this action and assigned to the new process, that CPU will be
 *          returned, if possible, to the original owner when the application
 *          ends.
 *          \n<B>Note:</B> By default, if this option is not selected, CPUs
 *          stolen and assigned to new applications will never be returned to
 *          their original owners.</DD>
 *
 *          <DT>--color[=\underline{WHEN}]</DT>
 *          <DD>Colorize the output; \underline{WHEN} can be 'yes'/'always' (default
 *          if parameter is omitted), 'auto' (default if option is omitted),
 *          or  'no'/'never'.</DD>
 *
 *          <DT>-h, --help</DT>
 *          <DD>Display this help.</DD>
 *      </DL>
 *  \section format CPU_LIST FORMAT
 *      The \underline{cpu_list} argument can be either a human readable list or
 *      range of decimal values,  e.g.: '0,5-7', or a hexadecimal or binary mask
 *      where the first CPU is the least significant bit, e.g.: '0xE1' or '0b11100001'.
 *      In all the examples above the argument means CPUs with logical id #0, #5, #6, #7.
 *
 *  \section author AUTHOR
 *      Barcelona Supercomputing Center (pm-tools@bsc.es)
 *  \section seealso SEE ALSO
 *      \ref dlb "dlb"(1), \ref dlb_run "dlb_run"(1), \ref dlb_shm "dlb_shm"(1)
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "apis/dlb.h"
#include "apis/dlb_drom.h"
#include "support/mask_utils.h"

#include <sched.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <ctype.h>
#include <regex.h>
#include <stdbool.h>

static int sys_size;

static void dlb_check(int error, pid_t pid, const char* func) {
    if (error) {
        if (pid) {
            fprintf(stderr, "Operation %s in PID %d did not succeed\n", func, pid);
        } else {
            fprintf(stderr, "Operation %s did not succeed\n", func);
        }
        fprintf(stderr, "Return code %d (%s)\n", error, DLB_Strerror(error));
        DLB_DROM_Detach();
        exit(EXIT_FAILURE);
    }
}

static void __attribute__((__noreturn__)) version(void) {
    fprintf(stdout, "%s %s (%s)\n", PACKAGE, VERSION, DLB_BUILD_VERSION);
    fprintf(stdout, "Configured with: %s\n", DLB_CONFIGURE_ARGS);
    exit(EXIT_SUCCESS);
}

static void __attribute__((__noreturn__)) usage(const char *program, FILE *out) {
    fprintf(out, "DLB - Dynamic Load Balancing, version %s.\n", VERSION);
    fprintf(out, (
                "usage:\n"
                "\t%1$s --list [--pid <pid>]\n"
                "\t%1$s --set <cpu_list> --pid <pid>\n"
                "\t%1$s --set <cpu_list> [--borrow] <application>\n"
                "\t%1$s --remove <cpu_list> [--pid <pid>]\n"
                "\n"
                ), program);

    fputs("Retrieve or modify the CPU affinity of DLB processes.\n\n", out);

    fputs((
                "Options:\n"
                "  -l, --list               list all DLB processes and their masks\n"
                "  -s, --set <cpu_list>     set affinity according to cpu_list\n"
                "  -c, --cpus <cpu_list>    same as --set\n"
                "  -r, --remove <cpu_list>  remove CPU ownership of any DLB process according to cpu_list\n"
                "  -p, --pid                operate only on existing given pid\n"
                "  -b, --borrow             stolen CPUs are recovered after process finalization\n"
                "  -h, --help               print this help\n"
                "\n"
                "<cpu_list> argument accepts the following formats:\n"
                "    Decimal numbers, comma-separated list and ranges allowed, e.g.: 0,5-7\n"
                "    Binary mask, being the first CPU the least significant bit, e.g.: 0b11100001\n"
                "    Hexadecimal mask, being the first CPU the least significant bit, e.g.: 0xE1\n"
                ), out);

    exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

static void set_affinity(pid_t pid, const cpu_set_t *new_mask) {
    DLB_DROM_Attach();
    int error = DLB_DROM_SetProcessMask(pid, new_mask, 0);
    dlb_check(error, pid, __FUNCTION__);
    DLB_DROM_Detach();

    fprintf(stdout, "PID %d's affinity set to: %s\n", pid, mu_to_str(new_mask));
}

static void execute(char **argv, const cpu_set_t *new_mask, bool borrow) {
    sched_setaffinity(0, sizeof(cpu_set_t), new_mask);
    pid_t pid = fork();
    if (pid < 0) {
        fprintf(stderr, "Failed to execute %s\n", argv[0]);
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        pid = getpid();
        DLB_DROM_Attach();
        dlb_drom_flags_t flags = DLB_STEAL_CPUS | (borrow ? DLB_RETURN_STOLEN : 0);
        int error = DLB_DROM_PreInit(pid, new_mask, flags, NULL);
        dlb_check(error, pid, __FUNCTION__);
        DLB_DROM_Detach();
        execvp(argv[0], argv);
        fprintf(stderr, "Failed to execute %s\n", argv[0]);
        exit(EXIT_FAILURE);
    } else {
        wait(NULL);
        DLB_DROM_Attach();
        int error = DLB_DROM_PostFinalize(pid, borrow);
        if (error != DLB_SUCCESS && error != DLB_ERR_NOPROC) {
            // DLB_ERR_NOPROC must be ignored here
            dlb_check(error, pid, __FUNCTION__);
        }
        DLB_DROM_Detach();
    }
}

static void remove_affinity_of_one(pid_t pid, const cpu_set_t *cpus_to_remove) {
    int i, error;

    // Get current PID's mask
    cpu_set_t pid_mask;
    error = DLB_DROM_GetProcessMask(pid, &pid_mask, 0);
    dlb_check(error, pid, __FUNCTION__);

    // Remove cpus from the PID's mask
    bool mask_dirty = false;
    for (i=0; i<sys_size; ++i) {
        if (CPU_ISSET(i, cpus_to_remove)) {
            CPU_CLR(i, &pid_mask);
            mask_dirty = true;
        }
    }

    // Apply final mask
    if (mask_dirty) {
        error = DLB_DROM_SetProcessMask(pid, &pid_mask, 0);
        dlb_check(error, pid, __FUNCTION__);
        fprintf(stdout, "PID %d's affinity set to: %s\n", pid, mu_to_str(&pid_mask));
    }
}

static void remove_affinity(pid_t pid, const cpu_set_t *cpus_to_remove) {
    DLB_DROM_Attach();

    if (pid) {
        remove_affinity_of_one(pid, cpus_to_remove);
    }
    else {
        // Get PID list from DLB
        int nelems;
        int *pidlist = malloc(sys_size*sizeof(int));
        DLB_DROM_GetPidList(pidlist, &nelems, sys_size);

        // Iterate pidlist
        int i;
        for (i=0; i<nelems; ++i) {
            remove_affinity_of_one(pidlist[i], cpus_to_remove);
        }
        free(pidlist);
    }
    DLB_DROM_Detach();
}

static void getpidof(int process_pseudo_id) {
    // Get PID list from DLB
    int nelems;
    int *pidlist = malloc(sys_size*sizeof(int));
    DLB_DROM_Attach();
    DLB_DROM_GetPidList(pidlist, &nelems, sys_size);
    // Iterate pidlist
    int i;
    for (i=0; i<nelems; ++i) {
        if (pidlist[i] > 0) {
            if (process_pseudo_id == 0) {
                fprintf(stdout, "%d\n", pidlist[i]);
                break;
            } else {
                --process_pseudo_id;
            }
        }
    }
    free(pidlist);
    DLB_DROM_Detach();
}

static void show_affinity(pid_t pid, int list_columns, dlb_printshmem_flags_t print_flags) {
    int error;
    cpu_set_t mask;

    DLB_DROM_Attach();
    if (pid) {
        // Show CPU affinity of given PID
        error = DLB_DROM_GetProcessMask(pid, &mask, 0);
        dlb_check(error, pid, __FUNCTION__);
        fprintf(stdout, "PID %d's current affinity CPU list: %s\n", pid, mu_to_str(&mask));
    } else {
        // Show CPU affinity of all processes attached to DLB
        DLB_PrintShmem(list_columns, print_flags);
    }
    DLB_DROM_Detach();
}


int main(int argc, char *argv[]) {
    bool do_list = false;
    bool do_set = false;
    bool do_remove = false;
    bool do_getpid = false;
    bool do_execute = false;
    bool borrow = false;

    dlb_printshmem_flags_t print_flags = DLB_COLOR_AUTO;
    int list_columns = 0;
    int process_pseudo_id = 0;
    pid_t pid = 0;
    cpu_set_t cpu_list;
    DLB_DROM_GetNumCpus(&sys_size);

    /* Long options that have no corresponding short option */
    enum {
        COLOR_OPTION = CHAR_MAX + 1
    };

    int opt;
    extern char *optarg;
    extern int optind;
    struct option long_options[] = {
        {"color",    optional_argument, NULL, COLOR_OPTION},
        {"list",     optional_argument, NULL, 'l'},
        {"set",      required_argument, NULL, 's'},
        {"cpus",     required_argument, NULL, 'c'},
        {"remove",   required_argument, NULL, 'r'},
        {"getpid",   required_argument, NULL, 'g'},
        {"pid",      required_argument, NULL, 'p'},
        {"borrow",   no_argument,       NULL, 'b'},
        {"help",     no_argument,       NULL, 'h'},
        {"version",  no_argument,       NULL, 'v'},
        {0,          0,                 NULL, 0 }
    };

    while ((opt = getopt_long(argc, argv, "+l::g:s:c:r:g:p:bhv", long_options, NULL)) != -1) {
        switch (opt) {
            case COLOR_OPTION:
                if (optarg && strcasecmp (optarg, "no") == 0) {
                    print_flags &= ~DLB_COLOR_AUTO;
                    print_flags &= ~DLB_COLOR_ALWAYS;
                } else {
                    print_flags |= DLB_COLOR_ALWAYS;
                }
                break;
            case 'l':
                do_list = true;
                if (optarg) {
                    list_columns = strtol(optarg, NULL, 0);
                }
                break;
            case 'c':
            case 's':
                do_set = true;
                mu_parse_mask(optarg, &cpu_list);
                break;
            case 'r':
                do_remove = true;
                mu_parse_mask(optarg, &cpu_list);
                break;
            case 'g':
                do_getpid = true;
                process_pseudo_id = strtoul(optarg, NULL, 10);
            case 'p':
                pid = strtoul(optarg, NULL, 10);
                break;
            case 'b':
                borrow = true;
                break;
            case 'h':
                usage(argv[0], stdout);
                break;
            case 'v':
                version();
                break;
            default:
                usage(argv[0], stderr);
                break;
        }
    }

    do_execute = argc > optind;

    // Incompatible options
    if ((do_execute && (do_list + do_remove + do_getpid + pid > 0)) ||
            (!do_execute && (do_list + do_set + do_remove + do_getpid != 1))) {
        usage(argv[0], stderr);
    }

    // Actions
    if (do_set && pid) {
        set_affinity(pid, &cpu_list);
    }
    else if (do_execute) {
        argv += optind;
        if (!do_set) sched_getaffinity(0, sizeof(cpu_set_t), &cpu_list);
        execute(argv, &cpu_list, borrow);
    }
    else if (do_remove) {
        remove_affinity(pid, &cpu_list);
    }
    else if (do_getpid) {
        getpidof(process_pseudo_id);
    }
    else if (do_list) {
        show_affinity(pid, list_columns, print_flags);
    }
    else {
        usage(argv[0], stderr);
    }

    return EXIT_SUCCESS;
}
