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

#include "support/mask_utils.h"
#include "apis/dlb_errors.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

int main( int argc, char **argv ) {
    fprintf(stderr, "System size: %d\n", mu_get_system_size());

    cpu_set_t process_mask;
    sched_getaffinity(0, sizeof(cpu_set_t), &process_mask);
    int last_cpu = -1;
    int i;
    for (i=CPU_SETSIZE; i>=0; --i) {
        if (CPU_ISSET(i, &process_mask)) {
            last_cpu = i;
            break;
        }
    }
    if (last_cpu) {
        fprintf(stderr, "Last CPU detected in the system: %d\n", last_cpu);
        assert( mu_get_system_size() == last_cpu + 1 );
    }

    cpu_set_t lb_mask1;
    mu_parse_mask("0-1,3,5-7", &lb_mask1);
    fprintf(stdout, "LB Mask: %s\n", mu_to_str(&lb_mask1));

    mu_init();
    fprintf(stdout, "System size: %d\n", mu_get_system_size());

    cpu_set_t lb_mask2;
    mu_parse_mask("0-1,3,5-7", &lb_mask2);
    fprintf(stdout, "LB Mask: %s\n", mu_to_str(&lb_mask2));

    // Test hexadecimal transformation
    cpu_set_t lb_mask3;
    mu_testing_set_sys_size(130);
    mu_parse_mask("0-1,4,7,31,63,127-129", &lb_mask3);
    char *out_str = mu_parse_to_slurm_format(&lb_mask3);
    fprintf(stdout, "Slurm mask: %s\n", out_str);
    assert(strcmp(out_str, "0x380000000000000008000000080000093") == 0);
    free(out_str);
    mu_finalize();

    assert(CPU_EQUAL(&lb_mask1, &lb_mask2));
    return 0;
}
