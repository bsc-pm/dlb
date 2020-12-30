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
    test_exec_fail=yes
</testinfo>*/

#include "unique_shmem.h"

#include "LB_comm/shmem.h"

/* Check different shared memory version */

struct data {
    int foo;
};

int main(int argc, char **argv) {
    struct data *shdata;
    shmem_handler_t *handler1 = shmem_init((void**)&shdata, sizeof(struct data),
            "cpuinfo", SHMEM_KEY, 1);
    shmem_handler_t *handler2 = shmem_init((void**)&shdata, sizeof(struct data),
            "cpuinfo", SHMEM_KEY, 2);

    /* This should not be reached, but it's here to ensure that the test fails
     * due to the shmem_init abort, and not due to the unfinished shmem destructor
     */
    shmem_finalize(handler1, SHMEM_DELETE);
    shmem_finalize(handler2, SHMEM_DELETE);

    return 0;
}
