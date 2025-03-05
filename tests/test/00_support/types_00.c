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

#include "support/types.h"

#include <stdio.h>
#include <string.h>
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
    assert(  equivalent_bool("false", "0") );
    assert( !equivalent_bool("false", "1") );

    err = parse_negated_bool("", &b_value);         assert(err);
    err = parse_negated_bool("null", &b_value);     assert(err);
    err = parse_negated_bool("1", &b_value);        assert(!err && !b_value);
    err = parse_negated_bool("yes", &b_value);      assert(!err && !b_value);
    err = parse_negated_bool("true", &b_value);     assert(!err && !b_value);
    err = parse_negated_bool("0", &b_value);        assert(!err && b_value);
    err = parse_negated_bool("no", &b_value);       assert(!err && b_value);
    err = parse_negated_bool("false", &b_value);    assert(!err && b_value);
    assert(  equivalent_negated_bool("false", "0") );
    assert( !equivalent_negated_bool("false", "1") );

    int i_value;
    err = parse_int("", &i_value);                  assert(err);
    err = parse_int("zzzzz", &i_value);             assert(err);
    err = parse_int("42", &i_value);                assert(!err && i_value==42);
    err = parse_int("42zzzzz", &i_value);           assert(!err && i_value==42);
    assert(  equivalent_int("0", "0") );
    assert( !equivalent_int("0", "1") );

    verbose_opts_t vb;
    parse_verbose_opts("", &vb);                    assert(vb==VB_CLEAR);
    parse_verbose_opts("null", &vb);                assert(vb==VB_CLEAR);
    parse_verbose_opts("yes", &vb);                 assert(vb==VB_ALL);
    parse_verbose_opts("api", &vb);                 assert(vb==VB_API);
    parse_verbose_opts("mpi_api", &vb);             assert(vb==VB_MPI_API);
    parse_verbose_opts("api:shmem", &vb);           assert(vb==(VB_API|VB_SHMEM));
    parse_verbose_opts("api:microlb:shmem:mpi_api:mpi_intercept:stats:drom:async", &vb);
    assert(vb == (VB_API | VB_MICROLB | VB_SHMEM | VB_MPI_API | VB_MPI_INT
                | VB_STATS | VB_DROM | VB_ASYNC));
    assert( strcmp(verbose_opts_tostr(VB_CLEAR), "no") == 0 );
    assert( strcmp(verbose_opts_tostr(VB_ALL), "all") == 0 );
    assert( strcmp(verbose_opts_tostr((verbose_opts_t)(VB_API|VB_SHMEM)), "api:shmem") == 0 );
    assert(  equivalent_verbose_opts("api:shmem", "shmem:api") );
    assert( !equivalent_verbose_opts("api:shmem", "shmem") );
    assert( !equivalent_verbose_opts("drom:async", "drom_async") );

    verbose_fmt_t fmt;
    parse_verbose_fmt("", &fmt);                    assert(fmt==VBF_CLEAR);
    parse_verbose_fmt("null", &fmt);                assert(fmt==VBF_CLEAR);
    parse_verbose_fmt("node", &fmt);                assert(fmt==VBF_NODE);
    parse_verbose_fmt("spid:mpinode", &fmt);        assert(fmt==(VBF_SPID|VBF_MPINODE));
    parse_verbose_fmt("node:spid:mpinode:mpirank:thread", &fmt);
    assert(fmt == (VBF_NODE | VBF_SPID | VBF_MPINODE | VBF_MPIRANK | VBF_THREAD));
    assert( strcmp(verbose_fmt_tostr(VBF_CLEAR), "") == 0 );
    assert( strcmp(verbose_fmt_tostr(VBF_NODE), "node") == 0 );
    assert( strcmp(verbose_fmt_tostr(VBF_MPINODE), "mpinode") == 0 );
    assert( strcmp(verbose_fmt_tostr((verbose_fmt_t)(VBF_NODE|VBF_MPINODE)), "node:mpinode") == 0 );
    assert(  equivalent_verbose_fmt("node:thread", "thread:node") );
    assert( !equivalent_verbose_fmt("node:thread", "thread") );
    assert( !equivalent_verbose_fmt("node:thread", "node_thread") );

    instrument_items_t inst;
    parse_instrument_items("", &inst);              assert(inst==INST_NONE);
    parse_instrument_items("null", &inst);          assert(inst==INST_NONE);
    parse_instrument_items("none", &inst);          assert(inst==INST_NONE);
    parse_instrument_items("all:none", &inst);      assert(inst==INST_ALL);
    parse_instrument_items("all:lewi", &inst);      assert(inst==INST_ALL);
    parse_instrument_items("null:mpi:lewi:drom:talp:barrier:ompt:cpus", &inst);
    assert(inst == (INST_MPI | INST_LEWI | INST_DROM | INST_TALP | INST_BARR
                | INST_OMPT | INST_CPUS));

    assert(strcmp(instrument_items_tostr(INST_NONE), "none") == 0);
    assert(strcmp(instrument_items_tostr(INST_ALL), "all") == 0);
    assert(strcmp(instrument_items_tostr((instrument_items_t)(INST_LEWI|INST_DROM)),
                "lewi:drom") == 0);
    assert(  equivalent_instrument_items("all", "thread:node:all") );
    assert( !equivalent_instrument_items("talp", "barrier") );
    assert( !equivalent_instrument_items("all", "callbacks") );

    lewi_affinity_t aff;
    err = parse_lewi_affinity("", &aff);                assert(err);
    err = parse_lewi_affinity("null", &aff);            assert(err);
    err = parse_lewi_affinity("auto", &aff);            assert(!err && aff==LEWI_AFFINITY_AUTO);
    err = parse_lewi_affinity("none", &aff);            assert(!err && aff==LEWI_AFFINITY_NONE);
    err = parse_lewi_affinity("mask", &aff);            assert(!err && aff==LEWI_AFFINITY_MASK);
    err = parse_lewi_affinity("any", &aff);             assert(!err && aff==LEWI_AFFINITY_MASK);
    err = parse_lewi_affinity("nearby-first", &aff);    assert(!err
                                                            && aff==LEWI_AFFINITY_NEARBY_FIRST);
    err = parse_lewi_affinity("nearby-only", &aff);     assert(!err
                                                            && aff==LEWI_AFFINITY_NEARBY_ONLY);
    err = parse_lewi_affinity("spread-ifempty", &aff);  assert(!err
                                                            && aff==LEWI_AFFINITY_SPREAD_IFEMPTY);
    assert(  equivalent_lewi_affinity("nearby-first", "nearby-first") );
    assert( !equivalent_lewi_affinity("nearby-first", "any") );

    policy_t pol;
    err = parse_policy("", &pol);                   assert(err);
    printf("Policy: %s\n", policy_tostr((policy_t)42));
    err = parse_policy("null", &pol);               assert(err);
    printf("Policy: %s\n", policy_tostr(pol));
    err = parse_policy("no", &pol);                 assert(!err && pol==POLICY_NONE);
    printf("Policy: %s\n", policy_tostr(pol));
    err = parse_policy("lewi", &pol);               assert(!err && pol==POLICY_LEWI);
    printf("Policy: %s\n", policy_tostr(pol));
    err = parse_policy("lewi_mask", &pol);          assert(!err && pol==POLICY_LEWI_MASK);
    printf("Policy: %s\n", policy_tostr(pol));
    assert(  equivalent_policy("lewi", "lewi") );
    assert( !equivalent_policy("lewi", "lewi_mask") );

    talp_summary_t summary;
    err = parse_talp_summary("", &summary);         assert(!err && summary==SUMMARY_NONE);
    err = parse_talp_summary("node", &summary);     assert(!err && summary==SUMMARY_NODE);
    err = parse_talp_summary("yes", &summary);      assert(!err && summary==SUMMARY_POP_METRICS);
    err = parse_talp_summary("all", &summary);      assert(!err && summary==SUMMARY_ALL);
    err = parse_talp_summary("pop", &summary);      assert(!err && summary==SUMMARY_NONE);
    assert( strcmp(talp_summary_tostr(SUMMARY_NONE), "none") == 0 );
    assert( strcmp(talp_summary_tostr(SUMMARY_ALL), "all") == 0 );
    assert( strcmp(talp_summary_tostr(SUMMARY_POP_METRICS), "pop-metrics") == 0 );
    assert(  equivalent_talp_summary("omp:pop-raw", "pop-raw:omp") );
    assert( !equivalent_talp_summary("pop-metrics", "pop-raw") );
    assert( !equivalent_talp_summary("node:process", "node_process") );

    talp_model_t talp_model;
    err = parse_talp_model("", &talp_model);
    assert( err );
    err = parse_talp_model("hybrid-v1", &talp_model);
    assert( !err && talp_model == TALP_MODEL_HYBRID_V1 );
    err = parse_talp_model("hybrid-v2", &talp_model);
    assert( !err && talp_model == TALP_MODEL_HYBRID_V2 );
    assert( strcmp(talp_model_tostr(TALP_MODEL_HYBRID_V2), "hybrid-v2") == 0 );
    assert(  equivalent_talp_model("hybrid-v1", "hybrid-v1") );
    assert( !equivalent_talp_model("hybrid-v1", "hybrid-v2") );

    interaction_mode_t mode;
    err = parse_mode("", &mode);                    assert(err);
    err = parse_mode("null", &mode);                assert(err);
    err = parse_mode("polling", &mode);             assert(!err && mode==MODE_POLLING);
    err = parse_mode("async", &mode);               assert(!err && mode==MODE_ASYNC);
    assert(  equivalent_mode("polling", "polling") );
    assert( !equivalent_mode("polling", "async") );

    mpi_set_t set;
    err = parse_mpiset("", &set);                   assert(err);
    err = parse_mpiset("all", &set);                assert(!err && set==MPISET_ALL);
    err = parse_mpiset("barrier", &set);            assert(!err && set==MPISET_BARRIER);
    assert(  equivalent_mpiset("all", "all") );
    assert( !equivalent_mpiset("all", "collectives") );

    omptool_opts_t ompt;
    err = parse_omptool_opts("", &ompt);            assert(!err && ompt==OMPTOOL_OPTS_NONE);
    err = parse_omptool_opts("lend", &ompt);        assert(!err && ompt==OMPTOOL_OPTS_LEND);
    err = parse_omptool_opts("lend:borrow", &ompt);
    assert(!err && ompt==(OMPTOOL_OPTS_LEND|OMPTOOL_OPTS_BORROW));
    assert(strcmp(omptool_opts_tostr(OMPTOOL_OPTS_BORROW), "borrow") == 0);
    assert(  equivalent_omptool_opts("lend:borrow", "borrow:borrow:lend") );
    assert( !equivalent_omptool_opts("lend", "borrow") );
    assert( !equivalent_omptool_opts("lend:borrow", "lend_borrow") );

    omptm_version_t omptm;
    err = parse_omptm_version("", &omptm);          assert(err);
    err = parse_omptm_version("omp5", &omptm);      assert(!err && omptm==OMPTM_OMP5);

    assert(  equivalent_omptm_version_opts("omp5", "omp5") );
    assert( !equivalent_omptm_version_opts("omp5", "free-agents") );

    return 0;
}
