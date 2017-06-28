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

#include "LB_core/DLB_kernel.h"
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
    add_always
} add_kind_t;

/* Implementation based on glibc's setenv:
 *  https://sourceware.org/git/?p=glibc.git;a=blob;f=stdlib/setenv.c
 */
static void add_to_environ(const char *name, const char *value, char ***environ, add_kind_t add) {
    const size_t namelen = strlen (name);
    size_t size = 0;
    char **ep;

    for (ep=*environ; *ep != NULL; ++ep) {
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
        char **new_environ = realloc(*environ, (size + 2)*sizeof(char*));
        if (new_environ) *environ = new_environ;
        // set last position of environ
        new_environ[size+1] = NULL;
        // pointer where new variable will be
        ep = new_environ + size;

    } else {
        /* Variable does exist */
        // ep already points to the variable we will overwrite
    }

    const size_t varlen = strlen(name) + 1 + strlen(value) + 1;
    char *np = malloc(varlen);
    snprintf(np, varlen, "%s=%s", name, value);
    *ep = np;
}

#pragma GCC visibility push(default)

int DLB_DROM_Init(void) {
    const options_t *options = get_global_options();
    shmem_cpuinfo_ext__init(options->shm_key);
    shmem_procinfo_ext__init(options->shm_key);
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

int DLB_DROM_GetProcessMask(int pid, dlb_cpu_set_t mask) {
    return shmem_procinfo__getprocessmask(pid, mask, QUERY_ASYNC);
}

int DLB_DROM_SetProcessMask(int pid, const_dlb_cpu_set_t mask) {
    return shmem_procinfo__setprocessmask(pid, mask, QUERY_ASYNC);
}

int DLB_DROM_GetProcessMask_sync(int pid, dlb_cpu_set_t mask) {
    return shmem_procinfo__getprocessmask(pid, mask, QUERY_SYNC);
}

int DLB_DROM_SetProcessMask_sync(int pid, const_dlb_cpu_set_t mask) {
    return shmem_procinfo__setprocessmask(pid, mask, QUERY_SYNC);
}

int DLB_DROM_PreInit(int pid, const_dlb_cpu_set_t mask, int steal, char ***environ) {
    // Obtain PreInit value
    char preinit_value[8];
    snprintf(preinit_value, 8, "%d", pid);

    // Obtain OMP_NUM_THREADS new value
    int new_num_threads = CPU_COUNT(mask);
    char omp_value[8];
    snprintf(omp_value, 8, "%d", new_num_threads);

    if (environ == NULL) {
        // These internal DLB variables are mandatory
        setenv("LB_DROM", "1", 1);
        setenv("LB_PREINIT_PID", preinit_value, 1);

        // If OMP_NUM_THREADS is set, it must match the current mask size
        const char *str = getenv("OMP_NUM_THREADS");
        if (str && strtol(str, NULL, 0) != new_num_threads) {
            warning("Re-setting OMP_NUM_THREADS to %d due to the new mask: %s",
                    new_num_threads, mu_to_str(mask));
            setenv("OMP_NUM_THREADS", omp_value, 1);
        }
    } else {
        // warning: environ must be a malloc'ed pointer
        add_to_environ("LB_DROM", "1", environ, add_always);
        add_to_environ("LB_PREINIT_PID", preinit_value, environ, add_always);
        add_to_environ("OMP_NUM_THREADS", omp_value, environ, add_only_if_present);
    }

    int error = shmem_procinfo_ext__preinit(pid, mask, steal);
    error = error ? error : shmem_cpuinfo_ext__preinit(pid, mask, steal);
    return error;
}

int DLB_DROM_PostFinalize(int pid, int return_stolen) {
    int error = shmem_procinfo_ext__postfinalize(pid, return_stolen);
    error = error ? error : shmem_cpuinfo_ext__postfinalize(pid);
    return error;
}

#pragma GCC visibility pop
