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
    test_generator_ENV=( "LB_TEST_MODE=single" )
</testinfo>*/

#include "support/mask_utils.h"

#include <sched.h>

int main(int argc, char *argv[]) {
    cpu_set_t system_mask;
    mu_get_system_mask(&system_mask);

    cpu_set_t affinity_mask;
    mu_get_affinity_mask(&affinity_mask, &system_mask, MU_ANY_BIT);
    mu_get_affinity_mask(&affinity_mask, &system_mask, MU_ALL_BITS);

    cpu_set_t zero_mask;
    CPU_ZERO(&zero_mask);
    mu_get_affinity_mask(&affinity_mask, &zero_mask, MU_ANY_BIT);
    mu_get_affinity_mask(&affinity_mask, &zero_mask, MU_ALL_BITS);

    mu_init();
    mu_finalize();
    return 0;
}
