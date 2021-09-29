/*********************************************************************************/
/*  Copyright 2009-2021 Barcelona Supercomputing Center                          */
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

#include "apis/dlb.h"
#include "apis/dlb_drom.h"
#include "support/mask_utils.h"

#include <sched.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>

/* Test DLB_PreInit */

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

    /* PreInit with full process mask */
    cpu_set_t process_mask;
    sched_getaffinity(0, sizeof(cpu_set_t), &process_mask);
    assert( DLB_PreInit(&process_mask, NULL) == DLB_SUCCESS );

    /* Init after binding master thread */
    cpu_set_t current_mask;
    CPU_ZERO(&current_mask);
    CPU_SET(sched_getcpu(), &current_mask);
    sched_setaffinity(0, sizeof(cpu_set_t), &current_mask);
    assert( DLB_Init(0, &current_mask, NULL) == DLB_SUCCESS );

    /* A second Init should return error */
    assert( DLB_Init(0, &current_mask, NULL) == DLB_ERR_INIT );

    /* Check current mask */
    assert( DLB_DROM_Attach() == DLB_SUCCESS );
    assert( DLB_DROM_GetProcessMask(getpid(), &current_mask, 0) == DLB_SUCCESS );
    assert( DLB_DROM_Detach() == DLB_SUCCESS );
    assert( CPU_EQUAL(&process_mask, &current_mask) );

    /* Finalize */
    assert( DLB_Finalize() == DLB_SUCCESS );

    return 0;
}
