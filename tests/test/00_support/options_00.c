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

#include "support/types.h"
#include "support/options.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

int main( int argc, char **argv ) {
    setenv("LB_DROM", "huh", 1);
    setenv("LB_MASK", "huh", 1);
    setenv("LB_LEND_MODE", "huh", 1);
    setenv("LB_VERBOSE", "huh", 1);
    setenv("LB_VERBOSE_FORMAT", "huh", 1);
    const char *dlb_args = "--policy=huh --drom=huh      --mask=huh   --lend-mode=huh "
        " this_should_be_ignored     --verbose=huh --verbose-format=huh";

    // Check initialization equivalency
    options_t options_1, options_2;
    options_init(&options_1, NULL);     // options_1 initialized with env. vars
    options_init(&options_2, dlb_args); // options_2 initialized with dlb_args
    assert(options_1.lewi == options_2.lewi);
    assert(options_1.drom == options_2.drom);
    assert(options_1.lewi_mpi == options_2.lewi_mpi);
    assert(options_1.verbose == options_2.verbose);
    assert(options_1.verbose_fmt == options_2.verbose_fmt);

    // Check some values
    options_init(&options_1, "--lewi=yes --drom=1 --lewi-affinity=nearby-only");
    assert(options_1.lewi == true);
    assert(options_1.drom == true);
    assert(options_1.lewi_affinity == PRIO_NEARBY_ONLY);

    // Check option overwrite
    options_init(&options_1, "--drom=1 --drom=0");
    assert(options_1.drom == false);

    // Check setter and getter
    char value[MAX_OPTION_LENGTH];
    options_init(&options_1, "--lewi-mpi-calls=barrier --shm-key=key --lend-mode=1cpu");
    assert(options_1.lewi_mpi_calls == MPISET_BARRIER);
    options_get_variable(&options_1, "--lewi-mpi-calls", value);
    assert(strcasecmp(value, "barrier") == 0);
    options_set_variable(&options_1, "--lewi-mpi-calls", "all");
    assert(options_1.lewi_mpi_calls == MPISET_ALL);
    options_get_variable(&options_1, "--lewi-mpi-calls", value);
    assert(strcasecmp(value, "all") == 0);
    options_get_variable(&options_1, "--shm-key", value);
    assert(strcasecmp(value, "key") == 0);
    int error_readonly_var = options_set_variable(&options_1, "LB_SHM_KEY", "new_key");
    assert(error_readonly_var);
    assert(strcasecmp(value, "key") == 0);
    options_get_variable(&options_1,"--lewi-mpi", value);
    assert(strcasecmp(value, "no") == 0);
    options_set_variable(&options_1,"--lewi-mpi", "1");
    assert(options_1.lewi_mpi == true);
    options_get_variable(&options_1,"--lewi-mpi", value);
    assert(strcasecmp(value, "yes") == 0);
    int error_unexisting_var = options_set_variable(&options_1, "FAKEVAR", "fakevalue");
    assert(error_unexisting_var);
    value[0] = '\0';
    error_unexisting_var = options_get_variable(&options_1, "FAKEVAR", value);
    assert(error_unexisting_var);
    assert(strcasecmp(value, "") == 0);


    // Print variables
    options_init(&options_1, "");
    options_print_variables(&options_1);

    // Unknown variables are silently ignored
    //options_init(&options_1, "--polic");

    // Bad format options trigger a SIGABRT
    //options_init(&options_1, "--policy");
    //options_init(&options_1, "--policy=");

    // Preference between environ variable and arguments
    setenv("LB_DROM", "1", 1);
    options_init(&options_1, "--drom=no");
    assert(options_1.drom == false);

    // TODO: some variables are still not being checked
    options_init(&options_1, "--mode=async");
    assert(options_1.mode == MODE_ASYNC);

    // Unset all variables and check that default values are preserved
    unsetenv("DLB_ARGS");
    unsetenv("LB_ARGS");
    unsetenv("LB_DROM");
    unsetenv("LB_MASK");
    unsetenv("LB_LEND_MODE");
    unsetenv("LB_VERBOSE");
    unsetenv("LB_VERBOSE_FORMAT");
    options_init(&options_1, NULL);
    options_init(&options_2, "");
    assert(options_1.verbose_fmt == options_2.verbose_fmt);

    // Check that different options are parsed using both methods
    setenv("DLB_ARGS", "--drom=1", 1);
    options_init(&options_1, "--lewi=1");
    assert(options_1.lewi == true);
    assert(options_1.drom == true);


    return 0;
}
