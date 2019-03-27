/*********************************************************************************/
/*  Copyright 2009-2018 Barcelona Supercomputing Center                          */
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
#include "support/mask_utils.h"

#include <assert.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/wait.h>

/* Create two shmems with different key */

enum { SHMEM_VERSION = 42 };

struct data {
    /* pthread_barrier_t barrier; */
    int foo;
};

int main(int argc, char **argv) {
    struct data *shdata1, *shdata2;
    shmem_handler_t *handler1, *handler2;

    // Create a shmem
    handler1 = shmem_init((void**)&shdata1, sizeof(struct data), "cpuinfo", SHMEM_KEY,
            SHMEM_VERSION);
    shdata1->foo = 1;

    // Try to create another shmem with the same name, it maps another region but same content
    handler2 = shmem_init((void**)&shdata2, sizeof(struct data), "cpuinfo", SHMEM_KEY,
            SHMEM_VERSION);
    assert( &shdata1 != &shdata2 );
    assert( shdata1->foo == shdata2->foo );

    // Create a shmem with custom key
    handler2 = shmem_init((void**)&shdata2, sizeof(struct data), "cpuinfo","42", SHMEM_VERSION);
    shdata2->foo = 2;
    assert( &shdata1 != &shdata2 );
    assert( shdata1->foo != shdata2->foo );

    shmem_finalize(handler1, SHMEM_DELETE);
    shmem_finalize(handler2, SHMEM_DELETE);

    return 0;
}
