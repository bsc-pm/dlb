/*********************************************************************************/
/*  Copyright 2009-2026 Barcelona Supercomputing Center                          */
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

#include "LB_core/DLB_kernel.h"
#include "LB_core/spd.h"
#include "dlb_errors.h"
#include "mngo/mngo_talp.h"
#include "support/mask_utils.h"
#include "unique_shmem.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

/* TLS global variable to store the spd pointer */
extern __thread subprocess_descriptor_t *thread_spd;

static char options[128];

void initialize(size_t id, const char *mask, subprocess_descriptor_t *spd) {
}

void finalize(subprocess_descriptor_t *spd) {
}

int main(int argc, char *argv[]) {
    enum { SYS_SIZE = 4 };
    mu_testing_set_sys_size(SYS_SIZE);

    enum { SHMEM_SIZE_MULTIPLIER = 1 };

    cpu_set_t process_mask;
    mu_parse_mask("1-4", &process_mask);

    size_t id = 1;

    {
        subprocess_descriptor_t spd;
        spd.id = id;
        thread_spd = &spd;

        strcpy(options, "--talp --shm-key=");
        strcat(options, SHMEM_KEY);

        assert(Initialize(&spd, id, 2, &process_mask, options) == DLB_SUCCESS);
        mngo_talp_region_t region;
        static char region_name[16] = "test";
        assert(mngo_talp__region_register(&spd, region_name, &region) == DLB_SUCCESS);
        assert(strcmp(mngo_talp__get_region_name(&region), region_name) == 0);

        mngo_talp_metrics_t metrics;
        mngo_talp__take_metrics(&spd, &region, &metrics);
        assert(Finish(&spd) == DLB_SUCCESS);
    }

    {
        subprocess_descriptor_t spd;
        spd.id = id;
        thread_spd = &spd;

        strcpy(options, "--shm-key=");
        strcat(options, SHMEM_KEY);

        assert(Initialize(&spd, id, 2, &process_mask, options) == DLB_SUCCESS);
        mngo_talp_region_t region;
        static char region_name[16] = "test";
        assert(mngo_talp__region_register(&spd, region_name, &region) == DLB_NOUPDT);
        assert(Finish(&spd) == DLB_SUCCESS);
    }

    return 0;    
}
