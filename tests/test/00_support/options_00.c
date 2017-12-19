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

#include "apis/dlb_errors.h"
#include "support/types.h"
#include "support/options.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

int main( int argc, char **argv ) {

    options_t options_1, options_2;

    // Check initialization equivalency
    const char *dlb_args = "--lewi --no-drom    this_should_be_ignored   --verbose=api     ";
    options_init(&options_1, dlb_args);         // options_1 initialized with dlb_args
    setenv("DLB_ARGS", "--lewi --non-existent-flag --no-drom --verbose=api", 1);
    options_init(&options_2, NULL);             // options_2 initialized with env. vars
    assert(options_1.lewi == options_2.lewi);
    assert(options_1.drom == options_2.drom);
    assert(options_1.lewi_mpi == options_2.lewi_mpi);
    assert(options_1.verbose == options_2.verbose);
    assert(options_1.verbose_fmt == options_2.verbose_fmt);

    // Check dlb_args precedence
    setenv("DLB_ARGS", "--no-lewi", 1);
    options_init(&options_1, "--lewi");
    assert(options_1.lewi == true);

    // Check that different options are parsed using both methods
    setenv("DLB_ARGS", "--drom", 1);
    options_init(&options_1, "--lewi");
    assert(options_1.lewi == true);
    assert(options_1.drom == true);

    unsetenv("DLB_ARGS");

    // Check some values
    options_init(&options_1, "--lewi=yes --drom=1 --lewi-affinity=nearby-only");
    assert(options_1.lewi == true);
    assert(options_1.drom == true);
    assert(options_1.lewi_affinity == PRIO_NEARBY_ONLY);

    // Check option overwrite
    options_init(&options_1, "--drom=1 --drom=0");
    assert(options_1.drom == false);

    // Check different forms of parsing a boolean
    options_init(&options_1, "--lewi");
    assert(options_1.lewi == true);
    options_init(&options_1, "--no-lewi");
    assert(options_1.lewi == false);
    options_init(&options_1, "--lewi=yes");
    assert(options_1.lewi == true);
    options_init(&options_1, "--lewi=no");
    assert(options_1.lewi == false);

    // Check setter and getter
    char value[MAX_OPTION_LENGTH];
    options_init(&options_1, "--no-lewi-mpi --lewi-mpi-calls=barrier --shm-key=key");
    assert(options_1.lewi_mpi_calls == MPISET_BARRIER);
    options_get_variable(&options_1, "--lewi-mpi-calls", value);
    assert(strcasecmp(value, "barrier") == 0);
    options_set_variable(&options_1, "--lewi-mpi-calls", "all");
    assert(options_1.lewi_mpi_calls == MPISET_ALL);
    options_get_variable(&options_1, "--lewi-mpi-calls", value);
    assert(strcasecmp(value, "all") == 0);
    options_get_variable(&options_1, "--shm-key", value);
    assert(strcasecmp(value, "key") == 0);
    int error_readonly_var = options_set_variable(&options_1, "--shm-key", "new_key");
    assert(error_readonly_var == DLB_ERR_PERM);
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
    assert(error_unexisting_var == DLB_ERR_NOENT);
    assert(strcasecmp(value, "") == 0);


    // Print variables
    options_init(&options_1, "");
    options_print_variables(&options_1);

    // Unknown variables are silently ignored
    //options_init(&options_1, "--polic");

    // Bad format options trigger a SIGABRT
    //options_init(&options_1, "--policy");
    //options_init(&options_1, "--policy=");

    // TODO: some variables are still not being checked
    options_init(&options_1, "--mode=async");
    assert(options_1.mode == MODE_ASYNC);

    // Unset all variables and check that default values are preserved
    options_init(&options_1, NULL);
    options_init(&options_2, "");
    assert(options_1.verbose_fmt == options_2.verbose_fmt);

    // Found bug: "--lewi --lewi-mpi" is well parsed but "--lewi-mpi --lewi" is not.
    options_init(&options_1, "--lewi --lewi-mpi");
    assert(options_1.lewi && options_1.lewi_mpi);
    options_init(&options_1, "--lewi-mpi --lewi");
    assert(options_1.lewi && options_1.lewi_mpi);

    return 0;
}
