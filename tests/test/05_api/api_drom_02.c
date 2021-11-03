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

#include <apis/dlb.h>
#include <apis/dlb_drom.h>
#include <support/env.h>

#include <sched.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>


/* DROM tests of a single process */

int main(int argc, char **argv) {
    /* Options */
    char options[64] = "--shm-key=";
    strcat(options, SHMEM_KEY);

    /* Modify DLB_ARGS to include options */
    dlb_setenv("DLB_ARGS", options, NULL, ENV_APPEND);

    int pid = getpid();
    cpu_set_t process_mask;
    sched_getaffinity(0, sizeof(cpu_set_t), &process_mask);

    /* Tests with an empty mask */
    {
        /* Save a copy of DLB_ARGS for further tests, since DROM_Attach adds the pre-init flag */
        char *dlb_args_copy = strdup(getenv("DLB_ARGS"));

        /* Preinitialize with an empty mask */
        cpu_set_t empty_mask;
        CPU_ZERO(&empty_mask);
        assert( DLB_DROM_Attach() == DLB_SUCCESS );
        assert( DLB_DROM_PreInit(pid, &empty_mask, DLB_STEAL_CPUS, NULL) == DLB_SUCCESS );

        /* DLB_Init */
        assert( DLB_Init(0, &process_mask, NULL) == DLB_SUCCESS );

        /* --drom is automatically set */
        char value[16];
        assert( DLB_GetVariable("--drom", value) == DLB_SUCCESS );
        assert( strcmp(value, "yes") == 0 );

        /* Process mask is still empty */
        cpu_set_t mask;
        assert( DLB_DROM_GetProcessMask(pid, &mask, 0) == DLB_SUCCESS );
        assert( CPU_COUNT(&mask) == 0 );

        /* Simulate set process mask from an external process */
        assert( DLB_DROM_SetProcessMask(pid, &process_mask, 0) == DLB_SUCCESS );
        assert( DLB_PollDROM(NULL, &mask) == DLB_SUCCESS );
        assert( CPU_EQUAL(&mask, &process_mask) );

        assert( DLB_Finalize() == DLB_SUCCESS );
        assert( DLB_DROM_Detach() == DLB_SUCCESS );

        /* Restore DLB_ARGS */
        setenv("DLB_ARGS", dlb_args_copy, 1);
        free(dlb_args_copy);
    }

    /* Test mask setter and getter from own process */
    {
        assert( DLB_Init(0, &process_mask, "--drom") == DLB_SUCCESS );

        /* Get mask */
        cpu_set_t mask;
        assert( DLB_DROM_GetProcessMask(0, &mask, 0) == DLB_SUCCESS );
        assert( CPU_EQUAL(&mask, &process_mask) );

        /* Set mask a new mask with 1 less CPU */
        int i;
        for (i=0; i<CPU_SETSIZE; ++i) {
            if (CPU_ISSET(i, &mask)) {
                CPU_CLR(i, &mask);
                break;
            }
        }
        assert( DLB_DROM_SetProcessMask(0, &mask, 0) == DLB_SUCCESS );
        assert( DLB_DROM_SetProcessMask(0, &mask, 0) == DLB_ERR_PDIRTY );
        cpu_set_t new_mask;
        assert( DLB_DROM_GetProcessMask(0, &new_mask, 0) == DLB_NOTED );
        assert( CPU_EQUAL(&mask, &new_mask) );
        assert( CPU_COUNT(&new_mask) + 1 == CPU_COUNT(&process_mask) );
        assert( DLB_DROM_GetProcessMask(0, &new_mask, 0) == DLB_SUCCESS );
        assert( CPU_EQUAL(&mask, &new_mask) );
        assert( CPU_COUNT(&new_mask) + 1 == CPU_COUNT(&process_mask) );

        assert( DLB_Finalize() == DLB_SUCCESS );
    }

    return 0;
}
