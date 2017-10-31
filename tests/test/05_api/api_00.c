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

/*<testinfo>
    test_generator="gens/basic-generator"
</testinfo>*/

#include "apis/dlb.h"

#include <sched.h>
#include <unistd.h>
#include <assert.h>

static cpu_set_t process_mask;

static void cb_enable_cpu(int cpuid) {
    CPU_SET(cpuid, &process_mask);
}

static void cb_disable_cpu(int cpuid) {
    CPU_CLR(cpuid, &process_mask);
    DLB_LendCpu(cpuid);
}

int main( int argc, char **argv ) {
    CPU_ZERO(&process_mask);
    CPU_SET(0, &process_mask);
    CPU_SET(1, &process_mask);

    assert( DLB_Init(0, &process_mask, "--policy=no") == DLB_SUCCESS );
    assert( DLB_CallbackSet(dlb_callback_disable_cpu, (dlb_callback_t)cb_disable_cpu)
            == DLB_SUCCESS);
    assert( DLB_CallbackSet(dlb_callback_enable_cpu, (dlb_callback_t)cb_enable_cpu)
            == DLB_SUCCESS);
    dlb_callback_t cb;
    assert( DLB_CallbackGet(dlb_callback_enable_cpu, &cb) == DLB_SUCCESS );
    assert( cb == (dlb_callback_t)cb_enable_cpu );

    // Basic enable-disable test
    assert( DLB_Disable() == DLB_SUCCESS );
    assert( DLB_Lend() == DLB_ERR_DISBLD );
    assert( DLB_Enable() == DLB_SUCCESS );
    assert( DLB_SetMaxParallelism(1) == DLB_SUCCESS );

    // Lend
    assert( DLB_Lend() == DLB_ERR_NOPOL );
    assert( DLB_LendCpu(0) == DLB_ERR_NOPOL );
    assert( DLB_LendCpus(1) == DLB_ERR_NOPOL );
    assert( DLB_LendCpuMask(&process_mask) == DLB_ERR_NOPOL );

    // Reclaim
    assert( DLB_Reclaim() == DLB_ERR_NOPOL );
    assert( DLB_ReclaimCpu(0) == DLB_ERR_NOPOL );
    assert( DLB_ReclaimCpus(1) == DLB_ERR_NOPOL );
    assert( DLB_ReclaimCpuMask(&process_mask) == DLB_ERR_NOPOL );

    // Acquire
    assert( DLB_AcquireCpu(0) == DLB_ERR_NOPOL );
    assert( DLB_AcquireCpuMask(&process_mask) == DLB_ERR_NOPOL );

    // Borrow
    assert( DLB_Borrow() == DLB_ERR_NOPOL );
    assert( DLB_BorrowCpu(1) == DLB_ERR_NOPOL );
    assert( DLB_BorrowCpus(1) == DLB_ERR_NOPOL );
    assert( DLB_BorrowCpuMask(&process_mask) == DLB_ERR_NOPOL );

    // Return
    assert( DLB_Return() == DLB_ERR_NOPOL );
    assert( DLB_ReturnCpu(0) == DLB_ERR_NOPOL );

    // Misc */
    assert( DLB_CheckCpuAvailability(0) == DLB_ERR_NOPOL );
    assert( DLB_Barrier() == DLB_SUCCESS );
    assert( DLB_PollDROM(NULL, NULL) == DLB_ERR_DISBLD );
    assert( DLB_SetVariable("LB_JUST_BARRIER", "1") == DLB_SUCCESS );
    char value[32];
    assert( DLB_GetVariable("LB_JUST_BARRIER", value) == DLB_SUCCESS );
    assert( DLB_PrintVariables() == DLB_SUCCESS );
    assert( DLB_PrintShmem() == DLB_SUCCESS );
    assert( DLB_Strerror(0) != NULL );

    assert( DLB_Finalize() == DLB_SUCCESS );

    // Check init again
    CPU_ZERO(&process_mask);
    CPU_SET(0, &process_mask);
    CPU_SET(1, &process_mask);
    assert( DLB_Init(0, &process_mask, NULL) == DLB_SUCCESS );
    assert( DLB_Init(0, &process_mask, NULL) == DLB_ERR_INIT );
    assert( DLB_Finalize() == DLB_SUCCESS );

    return 0;
}
