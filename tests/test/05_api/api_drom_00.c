/*********************************************************************************/
/*  Copyright 2009-2018 Barcelona Supercomputing Center                          */
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

/*<testinfo>
    test_generator="gens/basic-generator"
</testinfo>*/

#include "unique_shmem.h"

#include <apis/dlb_drom.h>
#include <LB_core/spd.h>

#include <sched.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* Basic DROM init / pre / post / finalize within 1 process */

static void test(const cpu_set_t *mask, char ***env) {
    pid_t pid = getpid();

    assert( DLB_DROM_Attach()                                   == DLB_SUCCESS );
    assert( DLB_DROM_PreInit(pid, mask, DLB_STEAL_CPUS, env)    == DLB_SUCCESS );
    assert( DLB_DROM_Detach()                                   == DLB_SUCCESS );

    /* Test that the spd has the proper shmem key
     * to check that DLB_ARGS has not benn overwritten
     */
    const subprocess_descriptor_t** spds = spd_get_spds();
    const subprocess_descriptor_t** spd = spds;
    while (*spd) {
        if ((*spd)->id == pid) {
            assert( strcmp((*spd)->options.shm_key, SHMEM_KEY) == 0 );
            break;
        }
    }
    free(spds);

    assert( DLB_DROM_Attach()                                   == DLB_SUCCESS );
    assert( DLB_DROM_PostFinalize(pid, 0)                       == DLB_SUCCESS );
    assert( DLB_DROM_Detach()                                   == DLB_SUCCESS );
}

int main(int argc, char **argv) {
    /* Options */
    char options[64] = "--shm-key=";
    strcat(options, SHMEM_KEY);

    /* Modify DLB_ARGS to include options */
    const char *dlb_args_env = getenv("DLB_ARGS");
    if (dlb_args_env) {
        size_t len = strlen(dlb_args_env) + 1 + strlen(options) + 1;
        char *new_dlb_args = malloc(sizeof(char)*len);
        sprintf(new_dlb_args, "%s %s", dlb_args_env, options);
        setenv("DLB_ARGS", new_dlb_args, 1);
        free(new_dlb_args);
    } else {
        setenv("DLB_ARGS", options, 1);
    }

    /* Process mask is [0] */
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(0, &mask);

    /* OMP_NUM_THREADS = 4 and it will have to be reduced to 1 because of the mask */
    setenv("OMP_NUM_THREADS", "4", 1);

    /* Fork with CURRENT environment */
    test(&mask, NULL);

    /* Re-set the original environment variables */
    setenv("DLB_ARGS", options, 1);
    setenv("OMP_NUM_THREADS", "4", 1);

    /* Fork with CUSTOM environment */
    char **new_environ = malloc(3*sizeof(char*)); // 2 variables + NULL
    // OMP_NUM_THREADS
    char var1[32] = "OMP_NUM_THREADS=";
    strcat(var1, getenv("OMP_NUM_THREADS"));
    const size_t var1len = strlen(var1) + 1;
    new_environ[0] = malloc(var1len);
    snprintf(new_environ[0], var1len, "%s", var1);
    // DLB_ARGS
    char var2[64] = "DLB_ARGS=";
    strcat(var2, getenv("DLB_ARGS"));
    const size_t var2len = strlen(var2) + 1;
    new_environ[1] = malloc(var2len);
    snprintf(new_environ[1], var2len, "%s", var2);
    //
    new_environ[2] = NULL;
    test(&mask, &new_environ);
    free(new_environ[0]);
    free(new_environ[1]);
    free(new_environ);

    return 0;
}
