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

/* Dynamic Resource Ownership Manager API */

#include "apis/dlb_drom.h"

#include "apis/DLB_interface.h"
#include "LB_comm/shmem_cpuinfo.h"
#include "LB_comm/shmem_procinfo.h"
#include "apis/dlb_errors.h"
#include "support/options.h"
#include "support/debug.h"
#include "support/mask_utils.h"

#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    add_only_if_present,
    add_always,
    append
} add_kind_t;

/* Implementation based on glibc's setenv:
 *  https://sourceware.org/git/?p=glibc.git;a=blob;f=stdlib/setenv.c
 */
static void add_to_environ(const char *name, const char *value, char ***next_environ,
        add_kind_t add) {
    const size_t namelen = strlen (name);
    size_t size = 0;
    char **ep;

    for (ep=*next_environ; *ep != NULL; ++ep) {
        if (!strncmp(*ep, name, namelen) && (*ep)[namelen] == '=')
            break;
        else
            ++size;
    }

    if (*ep == NULL) {
        /* Variable does not exist */
        if (add == add_only_if_present) return;

        /* Allocate new variable */
        // realloc (num_existing_vars + 1(new_var) + 1(NULL))
        char **new_environ = realloc(*next_environ, (size + 2)*sizeof(char*));
        if (new_environ) *next_environ = new_environ;
        // set last position of next_environ
        new_environ[size+1] = NULL;
        // pointer where new variable will be
        ep = new_environ + size;

    } else {
        /* Variable does exist */
        // ep already points to the variable we will overwrite

        /* Only if the variable exists and needs appending, allocate new buffer */
        if (add == append) {
            const size_t origlen = strlen(*ep);
            const size_t newlen = origlen + 1 + strlen(value) +1;
            char *new_np = realloc(*ep, newlen);
            if (new_np) *ep = new_np;
            sprintf(new_np+origlen, " %s", value);
            return;
        }
    }

    const size_t varlen = strlen(name) + 1 + strlen(value) + 1;
    char *np = malloc(varlen);
    snprintf(np, varlen, "%s=%s", name, value);
    *ep = np;
}

#pragma GCC visibility push(default)

int DLB_DROM_Init(void) {
    const char *shm_key;
    const options_t *global_options = get_global_options();
    if (global_options) {
        shm_key = global_options->shm_key;
    } else {
        options_t options;
        options_init(&options, NULL);
        shm_key = options.shm_key;
    }
    shmem_cpuinfo_ext__init(shm_key);
    shmem_procinfo_ext__init(shm_key);
    return DLB_SUCCESS;
}

int DLB_DROM_Finalize(void) {
    shmem_cpuinfo_ext__finalize();
    shmem_procinfo_ext__finalize();
    return DLB_SUCCESS;
}

int DLB_DROM_GetNumCpus(int *ncpus) {
    *ncpus = shmem_cpuinfo_ext__getnumcpus();
    return DLB_SUCCESS;
}

int DLB_DROM_GetPidList(int *pidlist, int *nelems, int max_len) {
    shmem_procinfo__getpidlist(pidlist, nelems, max_len);
    return DLB_SUCCESS;
}

int DLB_DROM_GetProcessMask(int pid, dlb_cpu_set_t mask, dlb_drom_flags_t flags) {
    return shmem_procinfo__getprocessmask(pid, mask, flags);
}

int DLB_DROM_SetProcessMask(int pid, const_dlb_cpu_set_t mask, dlb_drom_flags_t flags) {
    return shmem_procinfo__setprocessmask(pid, mask, flags);
}

int DLB_DROM_PreInit(int pid, const_dlb_cpu_set_t mask, dlb_drom_flags_t flags,
        char ***next_environ) {
    /* Set up DROM args */
    char drom_args[64];
    int __attribute__((unused)) n = snprintf(drom_args, 64, "--drom=1 --preinit-pid=%d %s",
            pid, flags & DLB_RETURN_STOLEN ? "--debug-opts=return-stolen" : "");
    ensure(n<64, "snprintf overflow");

    /* Set up OMP_NUM_THREADS new value */
    int new_num_threads = CPU_COUNT(mask);
    char omp_value[8];
    snprintf(omp_value, 8, "%d", new_num_threads);

    if (next_environ == NULL) {
        /* Append DROM args to DLB_ARGS */
        const char *dlb_args_env = getenv("DLB_ARGS");
        if (dlb_args_env) {
            size_t len = strlen(dlb_args_env) + 1 + strlen(drom_args) + 1;
            char *new_dlb_args = malloc(sizeof(char)*len);
            sprintf(new_dlb_args, "%s %s", dlb_args_env, drom_args);
            setenv("DLB_ARGS", new_dlb_args, 1);
            free(new_dlb_args);
        } else {
            setenv("DLB_ARGS", drom_args, 1);
        }

        /* Modify OMP_NUM_THREADS only if already set */
        const char *str = getenv("OMP_NUM_THREADS");
        if (str && strtol(str, NULL, 0) != new_num_threads) {
            warning("Re-setting OMP_NUM_THREADS to %d due to the new mask: %s",
                    new_num_threads, mu_to_str(mask));
            setenv("OMP_NUM_THREADS", omp_value, 1);
        }
    } else {
        // warning: next_environ must be a malloc'ed pointer
        add_to_environ("DLB_ARGS", drom_args, next_environ, append);
        add_to_environ("OMP_NUM_THREADS", omp_value, next_environ, add_only_if_present);
    }

    int error = shmem_procinfo_ext__preinit(pid, mask, flags & DLB_STEAL_CPUS);
    error = error ? error : shmem_cpuinfo_ext__preinit(pid, mask, flags & DLB_STEAL_CPUS);
    return error;
}

int DLB_DROM_PostFinalize(int pid, dlb_drom_flags_t flags) {
    int error = shmem_procinfo_ext__postfinalize(pid, flags & DLB_RETURN_STOLEN);
    error = error ? error : shmem_cpuinfo_ext__postfinalize(pid);
    return error;
}

int DLB_DROM_RecoverStolenCpus(int pid) {
    return shmem_procinfo_ext__recover_stolen_cpus(pid);
}

#pragma GCC visibility pop
