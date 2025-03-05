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

#include "LB_core/DLB_kernel.h"
#include "LB_core/spd.h"
#include "LB_comm/shmem_cpuinfo.h"
#include "LB_comm/shmem_procinfo.h"
/* #include "apis/dlb_talp.h" */
#include "apis/dlb_errors.h"
#include "support/mask_utils.h"
#include "support/options.h"

#include <sched.h>
#include <string.h>
/* #include <unistd.h> */
#include <stdio.h>
#include <assert.h>

/* Test DROM inheritance */


int main(int argc, char *argv[]) {

    enum { SHMEM_SIZE_MULTIPLIER = 1 };

    enum { SYS_SIZE = 4 };
    mu_testing_set_sys_size(SYS_SIZE);

    char options[64] = "--drom --shm-key=";
    strcat(options, SHMEM_KEY);

    /* parent process */
    cpu_set_t process_mask;
    mu_parse_mask("0-3", &process_mask);
    subprocess_descriptor_t spd = {.id = 111};
    options_init(&spd.options, options);
    memcpy(&spd.process_mask, &process_mask, sizeof(cpu_set_t));

    char children_options[80];
    snprintf(children_options, 80, "--drom --preinit-pid=%d --shm-key=%s", spd.id, SHMEM_KEY);

    /* child 0 */
    cpu_set_t child0_mask;
    mu_parse_mask("0", &child0_mask);
    subprocess_descriptor_t child0_spd = {.id = 1110};

    /* child 1 */
    cpu_set_t child1_mask;
    mu_parse_mask("1", &child1_mask);
    subprocess_descriptor_t child1_spd = {.id = 1111};

    /* child 2 */
    cpu_set_t child2_mask;
    mu_parse_mask("2", &child2_mask);
    subprocess_descriptor_t child2_spd = {.id = 1112};

    /* child 3 */
    cpu_set_t child3_mask;
    mu_parse_mask("3", &child3_mask);
    subprocess_descriptor_t child3_spd = {.id = 1113};

    /* Parent process pre-initializes [0-3], "child" processes inherit one by one */
    {
        /* DLB_DROM_Attach: */
        assert( shmem_cpuinfo_ext__init(SHMEM_KEY, 0)  == DLB_SUCCESS );
        assert( shmem_procinfo_ext__init(SHMEM_KEY, SHMEM_SIZE_MULTIPLIER) == DLB_SUCCESS );

        /* DLB_DROM_PreInit: */
        assert( shmem_procinfo_ext__preinit(spd.id, &process_mask, DLB_DROM_FLAGS_NONE)
                == DLB_SUCCESS );
        assert( shmem_cpuinfo_ext__preinit(spd.id, &process_mask, DLB_DROM_FLAGS_NONE)
                == DLB_SUCCESS );

        /* [Childs] DLB_Init: */
        assert( Initialize(&child0_spd, child0_spd.id, 0, &child0_mask, children_options)
                == DLB_SUCCESS );
        assert( Initialize(&child1_spd, child1_spd.id, 0, &child1_mask, children_options)
                == DLB_SUCCESS );
        assert( Initialize(&child2_spd, child2_spd.id, 0, &child2_mask, children_options)
                == DLB_SUCCESS );
        assert( Initialize(&child3_spd, child3_spd.id, 0, &child3_mask, children_options)
                == DLB_SUCCESS );

        /* [Childs] DLB_Finalize: */
        assert( Finish(&child0_spd) == DLB_SUCCESS );
        assert( Finish(&child1_spd) == DLB_SUCCESS );
        assert( Finish(&child2_spd) == DLB_SUCCESS );
        assert( Finish(&child3_spd) == DLB_SUCCESS );

        /* DLB_DROM_PostFinalize: */
        assert( shmem_procinfo_ext__postfinalize(spd.id, 0) == DLB_ERR_NOPROC );
        assert( shmem_cpuinfo_ext__postfinalize(spd.id)     == DLB_SUCCESS );

        /* DLB_DROM_Detach(): */
        assert( shmem_cpuinfo_ext__finalize()  == DLB_SUCCESS );
        assert( shmem_procinfo_ext__finalize() == DLB_SUCCESS );
    }

    return 0;
}
