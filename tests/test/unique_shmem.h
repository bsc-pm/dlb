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

#ifndef UNIQUE_SHMEM_T
#define UNIQUE_SHMEM_T

#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <signal.h>
#include <assert.h>

enum { SHMEM_KEY_MAX_LEN = 32 };
static char unique_shmem_key[SHMEM_KEY_MAX_LEN];
#define SHMEM_KEY unique_shmem_key

static bool test_and_delete_shmems(void) {
    bool shmem_exists = false;
    const char *const shmem_names[] = {
        "lewi", "cpuinfo", "procinfo", "async", "barrier", "mngo", "test"};
    enum { shmem_names_nelems = sizeof(shmem_names) / sizeof(shmem_names[0]) };
    enum { SHMEM_MAX_NAME_LENGTH = 64 };
    char shm_filename[SHMEM_MAX_NAME_LENGTH];
    int i;
    for (i=0; i<shmem_names_nelems; ++i) {
        snprintf(shm_filename, SHMEM_MAX_NAME_LENGTH,
                "/dev/shm/DLB_%s_%s", shmem_names[i], unique_shmem_key);
        if (access(shm_filename, F_OK) == 0) {
            fprintf(stderr, "ERROR: Shared memory %s still exists, removing...\n",
                    shm_filename);
            unlink(shm_filename);
            shmem_exists = true;
        }
    }
    return shmem_exists;
}

static void sighandler(int signum)
{
    if (signum == SIGABRT) {
        fprintf(stderr, "DLB TEST[%d]: SIGABRT captured\n", getpid());
        /* Clean up shared memories */
        test_and_delete_shmems();
        /* Call default SIGABRT handler since flag SA_RESETHAND is being used */
        raise(signum);
    }
}

__attribute__((constructor))
static void initialize_unique_shmem_key(void) {
    sprintf(unique_shmem_key, "testsuite_%d", getpid());
}


__attribute__((constructor))
static void delete_shmems_on_abort(void) {
    struct sigaction sa;
    sa.sa_handler = sighandler;
    sa.sa_flags = SA_RESETHAND;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGABRT, &sa, NULL);
}

__attribute__((destructor))
static void assert_noshm(void) {
    bool shmem_exists = test_and_delete_shmems();
    assert( !shmem_exists );
}

#endif /* UNIQUE_SHMEM_T */
