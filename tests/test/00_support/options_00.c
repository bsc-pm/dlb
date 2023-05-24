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

#include "apis/dlb_errors.h"
#include "support/types.h"
#include "support/options.h"
#include "LB_core/spd.h"

#include <limits.h>
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
    assert(options_1.lewi_keep_cpu_on_blocking_call == options_2.lewi_keep_cpu_on_blocking_call);
    assert(options_1.verbose == options_2.verbose);
    assert(options_1.verbose_fmt == options_2.verbose_fmt);

    // Check dlb_args precedence
    setenv("DLB_ARGS", "--no-lewi", 1);
    options_init(&options_1, "--lewi");
    assert(options_1.lewi == false);

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
    options_init(&options_1, "--talp");
    assert(options_1.talp == true);

    // Check option overwrite
    options_init(&options_1, "--drom=1 --drom=0");
    assert(options_1.drom == false);

    // Check option overwrite (werror because both arguments need to be cleaned)
    options_init(&options_1, "--debug-opts=werror --drom=1 --drom=0");
    assert(options_1.drom == false);
    options_init(&options_1, "    --debug-opts=werror   --drom=1   --drom=0  ");
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

    // Deprecated variables must be still assigned
    // But deprecated and unused variables must not fail, nor change any value
    memset(&options_1, 0, sizeof(options_t));
    memset(&options_2, 0, sizeof(options_t));
    options_init(&options_1, "--policy=lewi");
    options_init(&options_2, NULL);
    assert(memcmp(&options_1, &options_2, sizeof(options_t)) == 0);

    // Check setter and getter
    char value[MAX_OPTION_LENGTH];
    options_init(&options_1, "--lewi-mpi-calls=barrier --shm-key=key");
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
    options_get_variable(&options_1,"--lewi-keep-one-cpu", value);
    assert(strcasecmp(value, "no") == 0);
    options_set_variable(&options_1,"--lewi-keep-one-cpu", "1");
    assert(options_1.lewi_keep_cpu_on_blocking_call == true);
    options_get_variable(&options_1,"--lewi-keep-one-cpu", value);
    assert(strcasecmp(value, "yes") == 0);
    int error_unexisting_var = options_set_variable(&options_1, "FAKEVAR", "fakevalue");
    assert(error_unexisting_var);
    value[0] = '\0';
    error_unexisting_var = options_get_variable(&options_1, "FAKEVAR", value);
    assert(error_unexisting_var == DLB_ERR_NOENT);
    assert(strcasecmp(value, "") == 0);

    // Test deprecated variable negated with an existing one
    options_init(&options_1, "--lewi-mpi");
    assert(options_1.lewi_keep_cpu_on_blocking_call == false);
    options_init(&options_1, "--lewi-mpi=no");
    assert(options_1.lewi_keep_cpu_on_blocking_call == true);

    // Print variables
    options_init(&options_1, "");
    options_print_variables(&options_1, /* extended */ false);
    options_print_variables(&options_1, /* extended */ true);

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

    // Test particular options for --verbose and --instrument
    options_init(&options_1, "--instrument");
    assert(options_1.instrument == INST_ALL);
    options_init(&options_1, "--instrument=yes");
    assert(options_1.instrument == INST_ALL);
    options_init(&options_1, "--verbose");
    assert(options_1.verbose == VB_ALL);
    options_init(&options_1, "--verbose=yes");
    assert(options_1.verbose == VB_ALL);

    // Test --talp-output-file
    options_init(&options_1, "");
    assert(options_1.talp_output_file == NULL);
    options_finalize(&options_1);
    options_init(&options_1, "--talp-output-file=myfile.txt");
    assert(strcmp(options_1.talp_output_file, "myfile.txt") == 0);
    options_finalize(&options_1);

    // bug: "--lewi --lewi-greedy" is well parsed but "--lewi-greedy --lewi" is not.
    options_init(&options_1, "--lewi --lewi-greedy");
    assert(options_1.lewi && options_1.lewi_greedy);
    options_init(&options_1, "--lewi-greedy --lewi");
    assert(options_1.lewi && options_1.lewi_greedy);

    // bug: "--option=arg" is well parsed but "=arg" remains in the argument
    //      (werror will cause "Unrecognized options" warning to fail)
    options_init(&options_1, "--debug-opts=werror");
    assert(options_1.debug_opts & DBG_WERROR);

    // bug: "--option=" should not cause fatal errors, just warnings,
    // and the value must be the default
    options_init(&options_1, "--lewi-mpi-calls=");
    options_init(&options_2, "");
    assert(options_1.lewi_mpi_calls == options_2.lewi_mpi_calls);

    // Test individual entry parsing
    bool lewi;
    bool lewi_mpi;
    int barrier_id;
    char *shm_key = malloc(sizeof(char)*MAX_OPTION_LENGTH);
    char *talp_file = malloc(sizeof(char)*PATH_MAX);
    verbose_opts_t vb_opts;
    verbose_fmt_t vb_fmt;
    omptool_opts_t ompt_opts;
    instrument_items_t instr_items;
    mpi_set_t mpiset;
    priority_t prio;
    interaction_mode_t mode;
    talp_summary_t talp_sum;
    // 1) without thread_spd->options
    setenv("DLB_ARGS", "--lewi --lewi-mpi --barrier-id=3 --shm-key=custom_key"
            " --verbose=talp --lewi-ompt=mpi", 1);
    options_parse_entry("--lewi", &lewi);               assert(lewi == true);
    options_parse_entry("--lewi-mpi", &lewi_mpi);       assert(lewi_mpi == false);
    options_parse_entry("--barrier-id", &barrier_id);   assert(barrier_id == 3);
    options_parse_entry("--shm-key", shm_key);          assert(strcmp(shm_key, "custom_key") == 0);
    options_parse_entry("--verbose", &vb_opts);         assert(vb_opts == VB_TALP);
    options_parse_entry("--lewi-ompt", &ompt_opts);     assert(ompt_opts == OMPTOOL_OPTS_MPI);
    // other default values
    options_parse_entry("--verbose-format", &vb_fmt);   assert(vb_fmt == (VBF_NODE|VBF_SPID));
    options_parse_entry("--instrument", &instr_items);  assert(instr_items == INST_ALL);
    options_parse_entry("--lewi-mpi-calls", &mpiset);   assert(mpiset == MPISET_ALL);
    options_parse_entry("--lewi-affinity", &prio);      assert(prio == PRIO_NEARBY_FIRST);
    options_parse_entry("--mode", &mode);               assert(mode == MODE_POLLING);
    options_parse_entry("--talp-summary", &talp_sum);   assert(talp_sum == SUMMARY_POP_METRICS);
    // 2) with existing thread_spd
    spd_enter_dlb(NULL);
    options_init(&thread_spd->options, NULL);
    thread_spd->dlb_initialized = true;
    unsetenv("DLB_ARGS");
    options_parse_entry("--lewi", &lewi);               assert(lewi == true);
    options_parse_entry("--lewi-mpi", &lewi_mpi);       assert(lewi_mpi == false);
    options_parse_entry("--barrier-id", &barrier_id);   assert(barrier_id == 3);
    options_parse_entry("--shm-key", shm_key);          assert(strcmp(shm_key, "custom_key") == 0);
    options_parse_entry("--verbose", &vb_opts);         assert(vb_opts == VB_TALP);
    options_parse_entry("--lewi-ompt", &ompt_opts);     assert(ompt_opts == OMPTOOL_OPTS_MPI);
    // other default values
    options_parse_entry("--verbose-format", &vb_fmt);   assert(vb_fmt == (VBF_NODE|VBF_SPID));
    options_parse_entry("--instrument", &instr_items);  assert(instr_items == INST_ALL);
    options_parse_entry("--lewi-mpi-calls", &mpiset);   assert(mpiset == MPISET_ALL);
    options_parse_entry("--lewi-affinity", &prio);      assert(prio == PRIO_NEARBY_FIRST);
    options_parse_entry("--mode", &mode);               assert(mode == MODE_POLLING);
    options_parse_entry("--talp-summary", &talp_sum);   assert(talp_sum == SUMMARY_POP_METRICS);
    free(shm_key);
    free(talp_file);

    return 0;
}
