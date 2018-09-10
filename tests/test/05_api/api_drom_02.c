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

#include <apis/dlb.h>
#include <apis/dlb_drom.h>

#include <sched.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>


/* DROM test of processes preinitialized with an empty mask */

int main(int argc, char **argv) {
    int pid = getpid();
    cpu_set_t empty_mask = {.__bits={0}};

    /* Preinitialize with an empty mask */
    assert( DLB_DROM_Attach() == DLB_SUCCESS );
    assert( DLB_DROM_PreInit(pid, &empty_mask, DLB_STEAL_CPUS, NULL) == DLB_SUCCESS );

    /* DLB_Init */
    cpu_set_t process_mask;
    sched_getaffinity(0, sizeof(cpu_set_t), &process_mask);
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

    return 0;
}
