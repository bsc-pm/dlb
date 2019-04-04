/*********************************************************************************/
/*  Copyright 2009-2019 Barcelona Supercomputing Center                          */
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
    test_generator="gens/basic-generator -a --mode=polling|--mode=async"
</testinfo>*/

#include "assert_loop.h"
#include "unique_shmem.h"

#include "apis/dlb.h"
#include "apis/dlb_sp.h"
#include "support/mask_utils.h"

#include <assert.h>

enum {NSUBPROCS = 4};
static cpu_set_t sp_mask[NSUBPROCS];
static dlb_handler_t handlers[NSUBPROCS];
static interaction_mode_t mode;

static void cb_enable_cpu(int cpuid, void *arg) {
    int sp_id = *((int*)arg);
    cpu_set_t *mask = &sp_mask[sp_id];
    CPU_SET(cpuid, mask);
}

int main(int argc, char *argv[]) {
    int i;
    int err;

    // Options
    char options[64] = "--lewi --shm-key=";
    strcat(options, SHMEM_KEY);

    // test NSUBPROCS subprocesses, 1 CPU each, in a 16 CPU system
    enum { SYS_SIZE = 16 };
    mu_init();
    mu_testing_set_sys_size(SYS_SIZE);

    // Initialize subprocesses
    int sp_ids[NSUBPROCS];
    for (i=0; i<NSUBPROCS; ++i) {
        sp_ids[i] = i;
        CPU_ZERO(&sp_mask[i]);
        CPU_SET(i, &sp_mask[i]);
        handlers[i] = DLB_Init_sp(0, &sp_mask[i], options);
        assert( handlers[i] != NULL );
        assert( DLB_CallbackSet_sp(handlers[i], dlb_callback_enable_cpu,
                    (dlb_callback_t)cb_enable_cpu, &sp_ids[i]) == DLB_SUCCESS);
    }

    // Get interaction mode (this test is called twice, using mode={polling,async})
    char value[16];
    DLB_GetVariable_sp(handlers[0], "--mode", value);
    if (strcmp(value, "polling") == 0) {
        mode = MODE_POLLING;
    } else if (strcmp(value, "async") == 0) {
        mode = MODE_ASYNC;
    } else {
        // Unknown value
        return -1;
    }

    // Everyone requests 1 CPU
    for (i=0; i<NSUBPROCS; ++i) {
        err = DLB_AcquireCpus_sp(handlers[i], 1);
        assert( mode == MODE_ASYNC ? err == DLB_NOTED : err == DLB_NOUPDT );
    }

    // Everyone lends their own CPU and their requests are auto resolved
    for (i=0; i<NSUBPROCS; ++i) {
        CPU_CLR(i, &sp_mask[i]);
        assert( DLB_LendCpu_sp(handlers[i], i) == DLB_SUCCESS );
        // Acquire again if polling
        if (mode == MODE_POLLING) {
            assert( DLB_AcquireCpus_sp(handlers[i], 1) == DLB_SUCCESS );
        }
        assert_loop( CPU_ISSET(i, &sp_mask[i]) );
    }

    // Everyone requests SYS_SIZE CPUs
    for (i=0; i<NSUBPROCS; ++i) {
        err = DLB_AcquireCpus_sp(handlers[i], SYS_SIZE);
        assert( mode == MODE_ASYNC ? err == DLB_NOTED : err == DLB_NOUPDT );
    }

    // Everyone lends 1 CPU
    for (i=0; i<NSUBPROCS; ++i) {
        CPU_CLR(i, &sp_mask[i]);
        assert( DLB_LendCpu_sp(handlers[i], i) == DLB_SUCCESS );
    }

    // Every subprocess should receive all CPUs from the subprocesses still alive
    for (i=0; i<NSUBPROCS; ++i) {
        // If polling, ask again for as many CPUs are left
        if (mode == MODE_POLLING) {
            assert( DLB_AcquireCpus_sp(handlers[i], SYS_SIZE-i) == DLB_SUCCESS );
        }
        // Each subprocess should have all the CPUs left in the system, then finalizes
        assert_loop( CPU_COUNT(&sp_mask[i]) == NSUBPROCS-i );
        assert( DLB_Finalize_sp(handlers[i]) == DLB_SUCCESS );
    }

    return 0;
}
