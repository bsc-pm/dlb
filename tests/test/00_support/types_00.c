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

#include "support/types.h"

#include <stdio.h>
#include <unistd.h>
#include <assert.h>

int main(int argc, char *argv[]) {
    bool b_value;
    parse_bool("", &b_value);                   assert(!b_value);
    parse_bool("null", &b_value);               assert(!b_value);
    parse_bool("1", &b_value);                  assert(b_value);
    parse_bool("yes", &b_value);                assert(b_value);
    parse_bool("true", &b_value);               assert(b_value);
    parse_bool("0", &b_value);                  assert(!b_value);
    parse_bool("no", &b_value);                 assert(!b_value);
    parse_bool("false", &b_value);              assert(!b_value);

    int i_value;
    parse_int("42", &i_value);                  assert(i_value==42);

    blocking_mode_t mpi_mode;
    parse_blocking_mode("1cpu", &mpi_mode);     assert(mpi_mode==ONE_CPU);
    parse_blocking_mode("block", &mpi_mode);    assert(mpi_mode==BLOCK);

    verbose_opts_t vb;
    parse_verbose_opts("", &vb);                assert(vb==VB_CLEAR);
    parse_verbose_opts("null", &vb);            assert(vb==VB_CLEAR);
    parse_verbose_opts("api", &vb);             assert(vb&VB_API);
    parse_verbose_opts("api:shmem", &vb);       assert(vb&VB_API && vb&VB_SHMEM);
    parse_verbose_opts("api:microlb:shmem:mpi_api:mpi_intercept:stats:drom", &vb);
    assert(vb&VB_API && vb&VB_MICROLB && vb&VB_SHMEM && vb&VB_MPI_API
            && vb&VB_MPI_INT && vb&VB_STATS && vb&VB_DROM);

    verbose_fmt_t fmt;
    parse_verbose_fmt("", &fmt);                assert(fmt==VBF_CLEAR);
    parse_verbose_fmt("null", &fmt);            assert(fmt==VBF_CLEAR);
    parse_verbose_fmt("node", &fmt);            assert(fmt&VBF_NODE);
    parse_verbose_fmt("pid:mpinode", &fmt);     assert(fmt&VBF_PID && fmt&VBF_MPINODE);
    parse_verbose_fmt("node:pid:mpinode:mpirank:thread", &fmt);
    assert(fmt&VBF_NODE && fmt&VBF_PID && fmt&VBF_MPINODE && fmt&VBF_MPIRANK && fmt&VBF_THREAD);

    priority_t prio;
    parse_priority("", &prio);                  assert(prio==PRIO_AFFINITY_FIRST);
    parse_priority("null", &prio);              assert(prio==PRIO_AFFINITY_FIRST);
    parse_priority("none", &prio);              assert(prio==PRIO_NONE);
    parse_priority("affinity_first", &prio);    assert(prio==PRIO_AFFINITY_FIRST);
    parse_priority("affinity_full", &prio);     assert(prio==PRIO_AFFINITY_FULL);
    parse_priority("affinity_only", &prio);     assert(prio==PRIO_AFFINITY_ONLY);

    policy_t pol;
    parse_policy("", &pol);                     assert(pol==POLICY_NONE);
    printf("Policy: %s\n", policy_tostr(42));
    parse_policy("null", &pol);                 assert(pol==POLICY_NONE);
    printf("Policy: %s\n", policy_tostr(pol));
    parse_policy("none", &pol);                 assert(pol==POLICY_NONE);
    printf("Policy: %s\n", policy_tostr(pol));
    parse_policy("lewi", &pol);                 assert(pol==POLICY_LEWI);
    printf("Policy: %s\n", policy_tostr(pol));
    parse_policy("weight", &pol);               assert(pol==POLICY_WEIGHT);
    printf("Policy: %s\n", policy_tostr(pol));
    parse_policy("lewi_mask", &pol);            assert(pol==POLICY_LEWI_MASK);
    printf("Policy: %s\n", policy_tostr(pol));
    parse_policy("auto_lewi_mask", &pol);       assert(pol==POLICY_AUTO_LEWI_MASK);
    printf("Policy: %s\n", policy_tostr(pol));

    interaction_mode_t mode;
    parse_mode("", &mode);                      assert(mode==MODE_POLLING);
    parse_mode("null", &mode);                  assert(mode==MODE_POLLING);
    parse_mode("polling", &mode);               assert(mode==MODE_POLLING);
    parse_mode("async", &mode);                 assert(mode==MODE_ASYNC);

    return 0;
}
