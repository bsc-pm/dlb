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

/* Dynamic Resource Ownership Manager API */

#include "apis/dlb_drom.h"

#include "LB_comm/shmem_cpuinfo.h"
#include "LB_comm/shmem_procinfo.h"
#include "LB_core/spd.h"
#include "LB_core/DLB_kernel.h"
#include "apis/dlb_errors.h"
#include "support/options.h"
#include "support/debug.h"
#include "support/env.h"
#include "support/mask_utils.h"

#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#pragma GCC visibility push(default)

int DLB_DROM_Attach(void) {
    char shm_key[MAX_OPTION_LENGTH];
    options_parse_entry("--shm-key", &shm_key);
    int shmem_color;
    options_parse_entry("--lewi-color", &shmem_color);
    shmem_cpuinfo_ext__init(shm_key, shmem_color);
    shmem_procinfo_ext__init(shm_key);
    return DLB_SUCCESS;
}

int DLB_DROM_Detach(void) {
    int error = shmem_cpuinfo_ext__finalize();
    error = error ? error : shmem_procinfo_ext__finalize();
    return error;
}

int DLB_DROM_GetNumCpus(int *ncpus) {
    *ncpus = mu_get_system_size();
    return DLB_SUCCESS;
}

int DLB_DROM_GetPidList(int *pidlist, int *nelems, int max_len) {
    return shmem_procinfo__getpidlist(pidlist, nelems, max_len);
}

int DLB_DROM_GetProcessMask(int pid, dlb_cpu_set_t mask, dlb_drom_flags_t flags) {
    int error = shmem_procinfo__getprocessmask(pid, mask, flags);
    if (error == DLB_ERR_NOSHMEM) {
        DLB_DROM_Attach();
        error = shmem_procinfo__getprocessmask(pid, mask, flags);
        DLB_DROM_Detach();
    }
    return error;
}

int DLB_DROM_SetProcessMask(int pid, const_dlb_cpu_set_t mask, dlb_drom_flags_t flags) {
    spd_enter_dlb(thread_spd);
    int error = drom_setprocessmask(pid, mask, flags);
    if (error == DLB_ERR_NOSHMEM) {
        DLB_DROM_Attach();
        error = drom_setprocessmask(pid, mask, flags);
        DLB_DROM_Detach();
    }
    return error;
}

int DLB_DROM_PreInit(int pid, const_dlb_cpu_set_t mask, dlb_drom_flags_t flags,
        char ***next_environ) {
    /* Set up DROM args */
    char drom_args[64];
    int __attribute__((unused)) n = snprintf(drom_args, 64, "--drom=1 --preinit-pid=%d %s",
            pid, flags & DLB_RETURN_STOLEN ? "--debug-opts=return-stolen" : "");
    ensure(n<64, "snprintf overflow");
    dlb_setenv("DLB_ARGS", drom_args, next_environ, ENV_APPEND);

    /* Set up OMP_NUM_THREADS new value, only if mask is valid and variable is already set */
    int new_num_threads = mask ? CPU_COUNT(mask) : 0;
    char omp_value[8];
    snprintf(omp_value, 8, "%d", new_num_threads);
    if (new_num_threads > 0) {
        dlb_setenv("OMP_NUM_THREADS", omp_value, next_environ, ENV_UPDATE_IF_EXISTS);
    }

    int error = shmem_procinfo_ext__preinit(pid, mask, flags);
    error = error ? error : shmem_cpuinfo_ext__preinit(pid, mask, flags);
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
