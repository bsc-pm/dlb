/*********************************************************************************/
/*  Copyright 2009-2023 Barcelona Supercomputing Center                          */
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

#include "unique_shmem.h"

#include "LB_comm/shmem.h"
#include "LB_comm/shmem_talp.h"
#include "apis/dlb_errors.h"
#include "support/mask_utils.h"

#include <sched.h>
#include <sys/types.h>
#include <stdint.h>
#include <assert.h>

int main(int argc, char *argv[]) {
    pid_t p1_pid = 111;
    pid_t p2_pid = 222;

    assert( shmem_talp__finalize(42) == DLB_ERR_NOSHMEM );
    assert( shmem_talp__register(42, "", NULL) == DLB_ERR_NOSHMEM );
    assert( shmem_talp__getpidlist(NULL, NULL, 0) == DLB_ERR_NOSHMEM );
    assert( shmem_talp__getregionlist(NULL, NULL, 0, NULL) == DLB_ERR_NOSHMEM );
    assert( shmem_talp__get_times(0, NULL, NULL) == DLB_ERR_NOSHMEM );
    assert( shmem_talp__set_times(0, 0, 0) == DLB_ERR_NOSHMEM );

    /* Initialize shared memories */
    assert( shmem_talp__init(SHMEM_KEY, 0) == DLB_SUCCESS );    /* p1_pid and p2_pid */
    assert( shmem_talp__init(SHMEM_KEY, 0) == DLB_SUCCESS );
    assert( shmem_talp__exists() );

    /* Register some TALP regions */
    int region_id1 = -1;
    assert( shmem_talp__register(p1_pid, "Custom region 1", &region_id1) == DLB_SUCCESS );
    assert( region_id1 == 0 );
    assert( shmem_talp__set_times(region_id1, 111111, 222222) == DLB_SUCCESS );
    int64_t mpi_time = -1;
    int64_t useful_time = -1;
    assert( shmem_talp__get_times(region_id1, &mpi_time, &useful_time) == DLB_SUCCESS );
    assert( mpi_time == 111111 && useful_time == 222222);
    int region_id2 = -1 ;
    assert( shmem_talp__register(p1_pid, "Region 2", &region_id2) == DLB_SUCCESS );
    assert( region_id2 == 1 );
    assert( shmem_talp__set_times(region_id2, INT64_MAX, 42) == DLB_SUCCESS );
    int region_id3 = -1;
    assert( shmem_talp__register(p2_pid, "Custom region 1", &region_id3) == DLB_SUCCESS );
    assert( region_id3 == 2 );
    assert( shmem_talp__set_times(region_id3, 0, 4242) == DLB_SUCCESS );
    assert( shmem_talp__set_times(3, 0, 0) == DLB_ERR_NOENT );
    assert( shmem_talp__get_times(3, NULL, NULL) == DLB_ERR_NOENT );
    assert( shmem_talp__set_times(-1, 0, 0) == DLB_ERR_NOENT );
    assert( shmem_talp__get_times(-1, NULL, NULL) == DLB_ERR_NOENT );

    enum { max_len = 8 };
    int nelems;

    /* Get pidlist */
    pid_t pidlist[max_len];
    assert( shmem_talp__getpidlist(pidlist, &nelems, max_len) == DLB_SUCCESS );
    assert( nelems == 2 );
    assert( pidlist[0] == p1_pid && pidlist[1] == p2_pid );

    /* Get region id list */
    talp_region_list_t region_list[max_len];
    assert( shmem_talp__getregionlist(region_list, &nelems, max_len,
                "Custom region 1") == DLB_SUCCESS );
    assert( nelems == 2 );
    assert( region_list[0].pid == p1_pid && region_list[0].region_id == 0 );
    assert( region_list[1].pid == p2_pid && region_list[1].region_id == 2 );
    assert( shmem_talp__getregionlist(region_list, &nelems, max_len,
                "Region 2") == DLB_SUCCESS );
    assert( nelems == 1 );
    assert( region_list[0].pid == p1_pid && region_list[0].region_id == 1 );
    assert( shmem_talp__getregionlist(region_list, &nelems, max_len,
                "nonexistent region") == DLB_SUCCESS );
    assert( nelems == 0 );

    /* Fill up memory from id 3 until mu_get_system_size * 10 */
    int i;
    int expected_memory_capacity = mu_get_system_size() * 10;
    int region_id;
    for (i=3; i<expected_memory_capacity; ++i) {
        char name[32];
        snprintf(name, 32, "Region %d", i);
        assert( shmem_talp__register(p1_pid, name, &region_id) == DLB_SUCCESS );
        assert( region_id == i );
    }
    assert( shmem_talp__register(p1_pid, "No mem", &region_id) == DLB_ERR_NOMEM );
    assert( shmem_talp__set_times(i, 0, 0) == DLB_ERR_NOMEM );
    assert( shmem_talp__get_times(i, NULL, NULL) == DLB_ERR_NOMEM );

    /* Finalize shared memories */
    assert( shmem_talp__finalize(p1_pid) == DLB_SUCCESS );
    assert( shmem_talp__finalize(p2_pid) == DLB_SUCCESS );

    return 0;
}
