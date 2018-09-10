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

#ifndef ASSERT_NOSHM_H
#define ASSERT_NOSHM_H

#include <stdio.h>
#include <unistd.h>
#include <assert.h>

/*
 * Any source file that includes this header will:
 *  * Remove any shared memory file upon program start
 *  * Abort if any shared memory file exists upon program finalization
 */
enum { SHMEM_MAX_NAME_LENGTH = 64 };
static const char* const shmem_names[] = {"lewi", "cpuinfo", "procinfo", "async"};
enum { shmem_names_nelems = sizeof(shmem_names) / sizeof(shmem_names[0]) };

__attribute__((constructor)) static void delete_shm(void) {
    char shm_filename[SHMEM_MAX_NAME_LENGTH];
    int i;
    for (i=0; i<shmem_names_nelems; ++i) {
        snprintf(shm_filename, SHMEM_MAX_NAME_LENGTH,
                "/dev/shm/DLB_%s_%d", shmem_names[i], getuid());
        unlink(shm_filename);
    }
}

__attribute__((destructor)) static void assert_noshm(void) {
    char shm_filename[SHMEM_MAX_NAME_LENGTH];
    int i;
    for (i=0; i<shmem_names_nelems; ++i) {
        snprintf(shm_filename, SHMEM_MAX_NAME_LENGTH,
                "/dev/shm/DLB_%s_%d", shmem_names[i], getuid());
        assert( access(shm_filename, F_OK) == -1 );
    }
}

#endif /* ASSERT_NOSHM_H */
