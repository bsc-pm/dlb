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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "support/types.h"
#include "support/options.h"

int main( int argc, char **argv ) {
    setenv("LB_POLICY", "huh", 1);
    setenv("LB_DROM", "huh", 1);
    setenv("LB_MASK", "huh", 1);
    setenv("LB_LEND_MODE", "huh", 1);
    setenv("LB_VERBOSE", "huh", 1);
    setenv("LB_VERBOSE_FORMAT", "huh", 1);
    const char *lb_args = "--policy=huh --drom=huh      --mask=huh   --lend-mode=huh "
        " this_should_be_ignored     --verbose=huh --verbose-format=huh";

    // Check initialization equivalency
    options_t options_1, options_2;
    options_init(&options_1, NULL);     // options_1 initialized with env. vars
    options_init(&options_2, lb_args);  // options_2 initialized with lb_args
    assert(options_1.lb_policy == options_2.lb_policy);
    assert(options_1.drom == options_2.drom);
    assert(strcmp(options_1.mask, options_2.mask) == 0);
    assert(options_1.mpi_lend_mode == options_2.mpi_lend_mode);
    assert(options_1.verbose == options_2.verbose);
    assert(options_1.verbose_fmt == options_2.verbose_fmt);

    // Check some values
    options_init(&options_1, "--policy=lewi --drom=1 --priority=affinity_only");
    assert(options_1.lb_policy == POLICY_LEWI);
    assert(options_1.drom == true);
    assert(options_1.priority == PRIO_AFFINITY_ONLY);

    // Check option overwrite
    options_init(&options_1, "--drom=1 --drom=0");
    assert(options_1.drom == false);

    // Check setter and getter
    char value[MAX_OPTION_LENGTH];
    options_init(&options_1, "--just-barrier=1");
    assert(options_1.mpi_just_barrier == true);
    options_get_variable(&options_1, "--just-barrier", value);
    assert(strcasecmp(value, "true") == 0);
    options_set_variable(&options_1, "--just-barrier", "0");
    assert(options_1.mpi_just_barrier == false);
    options_get_variable(&options_1, "--just-barrier", value);
    assert(strcasecmp(value, "false") == 0);

    return 0;
}
