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

#include "assert_loop.h"
#include "unique_shmem.h"

#include "apis/dlb_errors.h"
#include "LB_core/spd.h"
#include "LB_policies/lewi_mask.h"
#include "LB_comm/shmem_procinfo.h"
#include "LB_comm/shmem_cpuinfo.h"
#include "LB_comm/shmem_async.h"
#include "LB_numThreads/numThreads.h"
#include "support/mask_utils.h"
#include "support/debug.h"

#include <sched.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>


/* Test race conditions between lewi requests and async helpers */

static subprocess_descriptor_t spd1;
static subprocess_descriptor_t spd2;
static cpu_set_t sp1_mask;
static cpu_set_t sp2_mask;

/* Subprocess 1 callbacks */
static void sp1_cb_enable_cpu(int cpuid, void *arg) {
    CPU_SET(cpuid, &sp1_mask);
}

static void sp1_cb_disable_cpu(int cpuid, void *arg) {
    lewi_mask_ReturnCpu(&spd1, cpuid);
    CPU_CLR(cpuid, &sp1_mask);
}

/* Subprocess 2 callbacks */
static void sp2_cb_enable_cpu(int cpuid, void *arg) {
    CPU_SET(cpuid, &sp2_mask);
}

static void sp2_cb_disable_cpu(int cpuid, void *arg) {
    lewi_mask_ReturnCpu(&spd2, cpuid);
    CPU_CLR(cpuid, &sp2_mask);
}

static void init_subprocess(subprocess_descriptor_t *spd, cpu_set_t *sp_mask,
        const cpu_set_t *process_mask, dlb_callback_t cb_enable, dlb_callback_t cb_disable) {
    static int id = 100;
    ++id;

    // Initialize subprocess mask
    memcpy(sp_mask, process_mask, sizeof(cpu_set_t));

    // Options
    char options[64] = "--verbose=shmem --mode=async --shm-key=";
    strcat(options, SHMEM_KEY);

    // Subprocess init
    spd->id = id;
    options_init(&spd->options, options);
    assert( spd->options.mode == MODE_ASYNC );
    debug_init(&spd->options);
    memcpy(&spd->process_mask, sp_mask, sizeof(cpu_set_t));
    assert( shmem_procinfo__init(spd->id, 0, &spd->process_mask, NULL, spd->options.shm_key)
            == DLB_SUCCESS );
    assert( shmem_cpuinfo__init(spd->id, 0, &spd->process_mask, spd->options.shm_key,
                spd->options.lewi_color) == DLB_SUCCESS );
    assert( shmem_async_init(spd->id, &spd->pm, &spd->process_mask, spd->options.shm_key)
            == DLB_SUCCESS );
    assert( pm_callback_set(&spd->pm, dlb_callback_enable_cpu, cb_enable, NULL) == DLB_SUCCESS );
    assert( pm_callback_set(&spd->pm, dlb_callback_disable_cpu, cb_disable, NULL) == DLB_SUCCESS );
    assert( lewi_mask_Init(spd) == DLB_SUCCESS );
}

static void finalize_subprocess(subprocess_descriptor_t *spd) {
    assert( lewi_mask_Finalize(spd) == DLB_SUCCESS );
    assert( shmem_cpuinfo__finalize(spd->id, spd->options.shm_key, spd->options.lewi_color)
            == DLB_SUCCESS );
    assert( shmem_procinfo__finalize(spd->id, false, spd->options.shm_key) == DLB_SUCCESS );
    assert( shmem_async_finalize(spd->id) == DLB_SUCCESS );
}

int main( int argc, char **argv ) {
    // This test needs at least room for 4 CPUs
    enum { SYS_SIZE = 4 };
    mu_init();
    mu_testing_set_sys_size(SYS_SIZE);

    // Initialize constant masks for fast reference
    const cpu_set_t sp1_process_mask = {.__bits={0x3}};   /* [0011] */
    const cpu_set_t sp2_process_mask = {.__bits={0xc}};   /* [1100] */

    // Sp1 finalizes with pending requests
    {
        // Init
        init_subprocess(&spd1, &sp1_mask, &sp1_process_mask,
                (dlb_callback_t)sp1_cb_enable_cpu, (dlb_callback_t)sp1_cb_disable_cpu);
        init_subprocess(&spd2, &sp2_mask, &sp2_process_mask,
                (dlb_callback_t)sp2_cb_enable_cpu, (dlb_callback_t)sp2_cb_disable_cpu);

        // Subprocess 1 wants to acquire CPU 3
        assert( lewi_mask_AcquireCpu(&spd1, 3) == DLB_NOTED );

        // Subprocess 1 finalizes
        finalize_subprocess(&spd1);

        // Subprocess 2 no longer needs CPU 3
        CPU_CLR(3, &sp2_mask);
        assert( lewi_mask_LendCpu(&spd2, 3) == DLB_SUCCESS );

        // Subprocess 2 finalizes
        finalize_subprocess(&spd2);
    }

    // Sp1 finalizes with reclaimed CPUs
    {
        // Init
        init_subprocess(&spd1, &sp1_mask, &sp1_process_mask,
                (dlb_callback_t)sp1_cb_enable_cpu, (dlb_callback_t)sp1_cb_disable_cpu);
        init_subprocess(&spd2, &sp2_mask, &sp2_process_mask,
                (dlb_callback_t)sp2_cb_enable_cpu, (dlb_callback_t)sp2_cb_disable_cpu);

        // Subprocess 1 lends CPU 1
        CPU_CLR(1, &sp1_mask);
        assert( lewi_mask_LendCpu(&spd1, 1) == DLB_SUCCESS );

        // Subprocess 2 wants to acquire CPU 1
        assert( lewi_mask_AcquireCpu(&spd2, 1) == DLB_SUCCESS );
        assert_loop( CPU_ISSET(1, &sp2_mask) );

        // Subprocess 1 reclaims CPU 1 and finalizes immediately
        finalize_subprocess(&spd1);

        // Subprocess 2 gets CPU 1 removed and cannot acquire it anymore
        assert_loop( !CPU_ISSET(1, &sp2_mask) );
        assert( lewi_mask_AcquireCpu(&spd2, 1) == DLB_ERR_PERM );

        // Subprocess 2 finalizes
        finalize_subprocess(&spd2);
    }

    return 0;
}
