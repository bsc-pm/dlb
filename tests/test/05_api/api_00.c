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
#include "support/mask_utils.h"

#include <sched.h>
#include <unistd.h>
#include <assert.h>

static cpu_set_t process_mask;

static void cb_enable_cpu(int cpuid, void *arg) {
    CPU_SET(cpuid, &process_mask);
}

static void cb_disable_cpu(int cpuid, void *arg) {
    CPU_CLR(cpuid, &process_mask);
    DLB_LendCpu(cpuid);
}

int main( int argc, char **argv ) {
    CPU_ZERO(&process_mask);
    CPU_SET(0, &process_mask);
    CPU_SET(1, &process_mask);

    assert( DLB_Init(0, &process_mask, "--no-lewi") == DLB_SUCCESS );
    assert( DLB_CallbackSet(dlb_callback_disable_cpu,
                (dlb_callback_t)cb_disable_cpu, NULL) == DLB_SUCCESS);
    assert( DLB_CallbackSet(dlb_callback_enable_cpu,
                (dlb_callback_t)cb_enable_cpu, NULL) == DLB_SUCCESS);
    dlb_callback_t cb;
    void *arg;
    assert( DLB_CallbackGet(dlb_callback_enable_cpu, &cb, &arg) == DLB_SUCCESS );
    assert( cb == (dlb_callback_t)cb_enable_cpu );
    assert( arg == NULL );

    // Basic enable-disable test
    assert( DLB_Disable() == DLB_SUCCESS );
    assert( DLB_Lend() == DLB_ERR_DISBLD );
    assert( DLB_Enable() == DLB_SUCCESS );
    assert( DLB_SetMaxParallelism(1) == DLB_ERR_NOPOL );

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

    // Misc
    assert( DLB_CheckCpuAvailability(0) == DLB_ERR_NOPOL );
    assert( DLB_Barrier() == DLB_SUCCESS );
    assert( DLB_PollDROM(NULL, NULL) == DLB_ERR_DISBLD );
    assert( DLB_SetVariable("--drom", "1") == DLB_ERR_PERM );
    assert( DLB_SetVariable("--debug-opts", "foo") == DLB_SUCCESS );
    char value[32];
    assert( DLB_GetVariable("--drom", value) == DLB_SUCCESS );
    assert( DLB_PrintVariables(0) == DLB_SUCCESS );
    assert( DLB_PrintShmem(0, 0) == DLB_SUCCESS );
    assert( DLB_Strerror(0) != NULL );

    assert( DLB_Finalize() == DLB_SUCCESS );

    // Check init again
    CPU_ZERO(&process_mask);
    CPU_SET(0, &process_mask);
    CPU_SET(1, &process_mask);
    assert( DLB_Init(0, &process_mask, NULL) == DLB_SUCCESS );
    assert( DLB_Init(0, &process_mask, NULL) == DLB_ERR_INIT );
    assert( DLB_Finalize() == DLB_SUCCESS );

    // Call DLB_PrintShmem with full node
    sched_getaffinity(0, sizeof(cpu_set_t), &process_mask);
    assert( DLB_Init(0, &process_mask, "--lewi") == DLB_SUCCESS );
    DLB_LendCpu(0);
    assert( DLB_PrintShmem(4, DLB_COLOR_AUTO) == DLB_SUCCESS );
    assert( DLB_Finalize() == DLB_SUCCESS );

    // Call DLB_PrintShmem with different sizes
    mu_testing_set_sys_size(64);
    CPU_ZERO(&process_mask);
    int i;
    for(i=0; i<64; ++i) CPU_SET(i, &process_mask);
    assert( DLB_Init(0, &process_mask, "--lewi") == DLB_SUCCESS );
    DLB_LendCpu(0);
    assert( DLB_PrintShmem(0, DLB_COLOR_AUTO) == DLB_SUCCESS );
    assert( DLB_Finalize() == DLB_SUCCESS );

    return 0;
}
