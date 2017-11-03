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

#include <stdio.h>
#include <unistd.h>
#include <assert.h>

int main(int argc, char *argv[]) {
    int err;

    bool b_value;
    err = parse_bool("", &b_value);                 assert(err);
    err = parse_bool("null", &b_value);             assert(err);
    err = parse_bool("1", &b_value);                assert(!err && b_value);
    err = parse_bool("yes", &b_value);              assert(!err && b_value);
    err = parse_bool("true", &b_value);             assert(!err && b_value);
    err = parse_bool("0", &b_value);                assert(!err && !b_value);
    err = parse_bool("no", &b_value);               assert(!err && !b_value);
    err = parse_bool("false", &b_value);            assert(!err && !b_value);

    int i_value;
    err = parse_int("", &i_value);                  assert(err);
    err = parse_int("zzzzz", &i_value);             assert(err);
    err = parse_int("42", &i_value);                assert(!err && i_value==42);
    err = parse_int("42zzzzz", &i_value);           assert(!err && i_value==42);

    blocking_mode_t mpi_mode;
    err = parse_blocking_mode("", &mpi_mode);       assert(err);
    err = parse_blocking_mode("1cpu", &mpi_mode);   assert(!err && mpi_mode==ONE_CPU);
    err = parse_blocking_mode("block", &mpi_mode);  assert(!err && mpi_mode==BLOCK);

    verbose_opts_t vb;
    parse_verbose_opts("", &vb);                    assert(vb==VB_CLEAR);
    parse_verbose_opts("null", &vb);                assert(vb==VB_CLEAR);
    parse_verbose_opts("api", &vb);                 assert(vb&VB_API);
    parse_verbose_opts("api:shmem", &vb);           assert(vb&VB_API && vb&VB_SHMEM);
    parse_verbose_opts("api:microlb:shmem:mpi_api:mpi_intercept:stats:drom:async", &vb);
    assert(vb&VB_API && vb&VB_MICROLB && vb&VB_SHMEM && vb&VB_MPI_API
            && vb&VB_MPI_INT && vb&VB_STATS && vb&VB_DROM && vb&VB_ASYNC);

    verbose_fmt_t fmt;
    parse_verbose_fmt("", &fmt);                    assert(fmt==VBF_CLEAR);
    parse_verbose_fmt("null", &fmt);                assert(fmt==VBF_CLEAR);
    parse_verbose_fmt("node", &fmt);                assert(fmt&VBF_NODE);
    parse_verbose_fmt("pid:mpinode", &fmt);         assert(fmt&VBF_PID && fmt&VBF_MPINODE);
    parse_verbose_fmt("node:pid:mpinode:mpirank:thread", &fmt);
    assert(fmt&VBF_NODE && fmt&VBF_PID && fmt&VBF_MPINODE && fmt&VBF_MPIRANK && fmt&VBF_THREAD);

    priority_t prio;
    err = parse_priority("", &prio);                assert(err);
    err = parse_priority("null", &prio);            assert(err);
    err = parse_priority("any", &prio);             assert(!err && prio==PRIO_ANY);
    err = parse_priority("nearby-first", &prio);    assert(!err && prio==PRIO_NEARBY_FIRST);
    err = parse_priority("nearby-only", &prio);     assert(!err && prio==PRIO_NEARBY_ONLY);
    err = parse_priority("spread-ifempty", &prio);  assert(!err && prio==PRIO_SPREAD_IFEMPTY);

    policy_t pol;
    err = parse_policy("", &pol);                   assert(err);
    printf("Policy: %s\n", policy_tostr(42));
    err = parse_policy("null", &pol);               assert(err);
    printf("Policy: %s\n", policy_tostr(pol));
    err = parse_policy("no", &pol);                 assert(!err && pol==POLICY_NONE);
    printf("Policy: %s\n", policy_tostr(pol));
    err = parse_policy("lewi", &pol);               assert(!err && pol==POLICY_LEWI);
    printf("Policy: %s\n", policy_tostr(pol));
    err = parse_policy("lewi_mask", &pol);          assert(!err && pol==POLICY_LEWI_MASK);
    printf("Policy: %s\n", policy_tostr(pol));

    interaction_mode_t mode;
    err = parse_mode("", &mode);                    assert(err);
    err = parse_mode("null", &mode);                assert(err);
    err = parse_mode("polling", &mode);             assert(!err && mode==MODE_POLLING);
    err = parse_mode("async", &mode);               assert(!err && mode==MODE_ASYNC);

    return 0;
}
