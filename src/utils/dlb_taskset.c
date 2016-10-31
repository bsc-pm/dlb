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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <sched.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <ctype.h>
#include <regex.h>
#include <stdbool.h>
#include "support/mask_utils.h"
#include "LB_core/DLB_interface.h"

static int sys_size;

static void dlb_check(int error, pid_t pid, const char* func) {
    if (error) {
        if (pid) {
            fprintf(stderr, "Operation %s in PID %d did not succeed\n", func, pid);
        } else {
            fprintf(stderr, "Operation %s did not succeed\n", func);
        }
        DLB_Drom_Finalize();
        exit(EXIT_FAILURE);
    }
}

static void __attribute__((__noreturn__)) usage(const char * program, FILE *out) {
    fprintf(out, "DLB - Dynamic Load Balancing, version %s.\n", VERSION);
    fprintf(out, (
                "usage:\n"
                "\t%1$s [-l|--list] [-p <pid>]\n"
                "\t%1$s [-s|--set <cpu_list> -p <pid>]\n"
                "\t%1$s [-s|--set <cpu_list> program]\n"
                "\t%1$s [-r|--remove <cpu_list>] [-p <pid>]\n\n"
                ), program);

    fputs("Retrieve or modify the CPU affinity of DLB processes.\n\n", out);

    fputs((
                "Options:\n"
                "  -l, --list               list all DLB processes and their masks\n"
                "  -s, --set <cpu_list>     set affinity according to cpu_list\n"
                "  -c, --cpus <cpu_list>    same as --set\n"
                "  -r, --remove <cpu_list>  remove CPU ownership of any DLB process according to cpu_list\n"
                "  -p, --pid                operate only on existing given pid\n"
                "  -y, --sync               use synchronous operations\n"
                "  -h, --help               print this help\n"
                "\n"
                "<cpu_list> argument accepts the following formats:\n"
                "    Decimal numbers, comma-separated list and ranges allowed, e.g.: 0,5-7\n"
                "    Binary mask, being the first CPU the most significative bit, e.g.: 10000111b\n"
                ), out);

    exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

static void get_cpus(unsigned int ncpus) {
    int max_len = sys_size;
    int cpulist[max_len];
    int nelems;
    int steal = 1;

    DLB_Drom_Init();
    DLB_Drom_getCPUs(ncpus, steal, cpulist, &nelems, max_len);
    DLB_Drom_Finalize();

    char hostname[HOST_NAME_MAX];
    gethostname(hostname, HOST_NAME_MAX);

    // Let's assume a maximum size of cpuid of 4 digits, plus 1 space per cpu, plus \0
    char *cpulist_str = (char*) malloc((nelems*5+1)*sizeof(char));
    int i, j;
    for (i=0, j=0; i<nelems; ++i) {
        j += sprintf(&cpulist_str[j], "%d ", cpulist[i]);
    }
    cpulist_str[j] = '\0';

    fprintf(stdout, "%s %s\n", hostname, cpulist_str);
}

static void set_affinity(pid_t pid, const cpu_set_t *new_mask, bool sync) {
    DLB_Drom_Init();
    int error = sync ?
        DLB_Drom_SetProcessMask_sync(pid, new_mask) :
        DLB_Drom_SetProcessMask(pid, new_mask);
    dlb_check(error, pid, __FUNCTION__);
    DLB_Drom_Finalize();

    fprintf(stdout, "PID %d's affinity set to: %s\n", pid, mu_to_str(new_mask));
}

static void __attribute__((__noreturn__)) execute(char **argv, const cpu_set_t *new_mask) {
    sched_setaffinity(0, sizeof(cpu_set_t), new_mask);
    DLB_Drom_Init();
    pid_t pid = getpid();
    int error = DLB_Drom_PreRegister(pid, new_mask, 1);
    dlb_check(error, pid, __FUNCTION__);
    DLB_Drom_Finalize();
    execvp(argv[0], argv);
    fprintf(stderr, "Failed to execute %s\n", argv[0]);
    exit(EXIT_FAILURE);
}

static void remove_affinity_of_one(pid_t pid, const cpu_set_t *cpus_to_remove, bool sync) {
    int i, error;

    // Get current PID's mask
    cpu_set_t pid_mask;
    error = sync ?
        DLB_Drom_GetProcessMask_sync(pid, &pid_mask) :
        DLB_Drom_GetProcessMask(pid, &pid_mask);
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
        error = sync ?
            DLB_Drom_SetProcessMask_sync(pid, &pid_mask) :
            DLB_Drom_SetProcessMask(pid, &pid_mask);
        dlb_check(error, pid, __FUNCTION__);
        fprintf(stdout, "PID %d's affinity set to: %s\n", pid, mu_to_str(&pid_mask));
    }
}

static void remove_affinity(pid_t pid, const cpu_set_t *cpus_to_remove, bool sync) {
    DLB_Drom_Init();

    if (pid) {
        remove_affinity_of_one(pid, cpus_to_remove, sync);
    }
    else {
        // Get PID list from DLB
        int nelems;
        int *pidlist = (int*) malloc(sys_size*sizeof(int));
        DLB_Drom_GetPidList(pidlist, &nelems, sys_size);

        // Iterate pidlist
        int i;
        for (i=0; i<nelems; ++i) {
            remove_affinity_of_one(pidlist[i], cpus_to_remove, sync);
        }
        free(pidlist);
    }
    DLB_Drom_Finalize();
}

static void show_affinity(pid_t pid, bool sync) {
    int error;
    cpu_set_t mask;

    DLB_Drom_Init();
    if (pid) {
        // Show CPU affinity of given PID
        error = sync ?
            DLB_Drom_GetProcessMask_sync(pid, &mask) :
            DLB_Drom_GetProcessMask(pid, &mask);
        dlb_check(error, pid, __FUNCTION__);
        fprintf(stdout, "PID %d's current affinity CPU list: %s\n", pid, mu_to_str(&mask));
    } else {
        // Show CPU affinity of all processes attached to DLB
        DLB_PrintShmem();
    }
    DLB_Drom_Finalize();
}


int main(int argc, char *argv[]) {
    bool do_list = false;
    bool do_set = false;
    bool do_remove = false;
    bool do_get = false;

    pid_t pid = 0;
    cpu_set_t cpu_list;
    unsigned int ncpus = 0;
    sys_size = DLB_Drom_GetNumCpus();
    bool sync = false;

    int opt;
    extern char *optarg;
    extern int optind;
    struct option long_options[] = {
        {"list",     no_argument,       0, 'l'},
        {"get",      required_argument, 0, 'g'},
        {"set",      required_argument, 0, 's'},
        {"cpus",     required_argument, 0, 'c'},
        {"remove",   required_argument, 0, 'r'},
        {"pid",      required_argument, 0, 'p'},
        {"sync",     no_argument,       0, 'y'},
        {"help",     no_argument,       0, 'h'},
        {0,          0,                 0, 0 }
    };

    while ((opt = getopt_long(argc, argv, "lg:s:c:r:p:yh", long_options, NULL)) != -1) {
        switch (opt) {
        case 'l':
            do_list = true;
            break;
        case 'g':
            do_get = true;
            ncpus = strtoul(optarg, NULL, 10);
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
        case 'p':
            pid = strtoul(optarg, NULL, 10);
            break;
        case 'y':
            sync = true;
            break;
        case 'h':
            usage(argv[0], stdout);
            break;
        default:
            usage(argv[0], stderr);
            break;
        }
    }

    // Actions
    if (do_get) {
        get_cpus(ncpus);
    }
    else if (do_set && pid) {
        set_affinity(pid, &cpu_list, sync);
    }
    else if (do_set && !pid && argc > optind) {
        argv += optind;
        execute(argv, &cpu_list);
    }
    else if (do_remove) {
        remove_affinity(pid, &cpu_list, sync);
    }
    else if (do_list || pid) {
        show_affinity(pid, sync);
    }
    else {
        usage(argv[0], stderr);
    }

    return EXIT_SUCCESS;
}
