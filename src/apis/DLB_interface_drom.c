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

#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    shmem_procinfo_ext__getpidlist(pidlist, nelems, max_len);
    return DLB_SUCCESS;
}

int DLB_DROM_GetProcessMask(int pid, dlb_cpu_set_t mask) {
    return shmem_procinfo_ext__getprocessmask(pid, mask);
}

int DLB_DROM_SetProcessMask(int pid, const_dlb_cpu_set_t mask) {
    return shmem_procinfo_ext__setprocessmask(pid, mask);
}

// Unmaintain?
int DLB_DROM_GetCpus(int ncpus, int steal, int *cpulist, int *nelems, int max_len) {
    return shmem_procinfo_ext__getcpus(ncpus, steal, cpulist, nelems, max_len);
}

int DLB_DROM_PreInit(int pid, const_dlb_cpu_set_t mask, int steal, char ***environ) {
    char env_var[] = "LB_PREINIT";
    char env_val[8];
    snprintf(env_val, 8, "%d", pid);
    if (environ == NULL) {
        setenv(env_var, env_val, 1);
    } else {
        // warning: environ must be a malloc'ed pointer and must not contain LB_PREINIT already
        size_t size = 0;
        char **ep;
        // size will contain the number of existing variables
        for (ep=*environ; *ep != NULL; ++ep) ++size;
        // realloc (num_existing_vars + 1(new_var) + 1(NULL))
        char **new_environ = (char**) realloc(*environ, (size + 2)*sizeof(char*));
        if (new_environ == NULL) return -1;
        // set last position of environ
        new_environ[size+1] = NULL;
        // pointer where new variable will be
        ep = new_environ + size;

        const size_t varlen = strlen(env_var) + 1 + strlen(env_val) + 1;
        char *np = malloc(varlen);
        snprintf(np, varlen, "%s=%s", env_var, env_val);
        *ep = np;

        *environ = new_environ;
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
