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

#include "LB_comm/shmem_async.h"
#include "LB_comm/shmem_barrier.h"
#include "LB_comm/shmem_cpuinfo.h"
#include "LB_comm/shmem_procinfo.h"
#include "support/mask_utils.h"

#include <sched.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>

// Check versions of all shmems


static void check_async_version(void) {
    enum { KNOWN_ASYNC_VERSION = 2 };
    enum { KNOWN_QUEUE_SIZE = 100 };
    struct KnownMessage {
        enum {ENUM1} enum1;
        int int1;
    };
    struct KnownAsyncShdata {
        /* Queue attributes */
        struct KnownMessage message[KNOWN_QUEUE_SIZE];
        unsigned int uint1;
        unsigned int uint2;
        pthread_mutex_t mutex;
        pthread_cond_t  cond;

        /* Helper metadata */
        pid_t pid;
        pthread_t pth;
        cpu_set_t mask;
        void *ptr1;
        bool bool1;
    };

    int version = shmem_async__version();
    size_t size = shmem_async__size();
    size_t known_size = sizeof(struct KnownAsyncShdata) * mu_get_system_size();
    fprintf(stderr, "shmem_async version %d, size: %lu, known_size: %lu\n",
            version, size, known_size);
    assert( version == KNOWN_ASYNC_VERSION );
    assert( size == known_size );
}

static void check_barrier_version(void) {
    enum { KNOWN_BARRIER_VERSION = 1 };
    struct KnownBarrierShdata {
        bool bool1;
        int int1;
        int int2;
        sem_t sem;
    };

    int version = shmem_barrier__version();
    size_t size = shmem_barrier__size();
    size_t known_size = sizeof(struct KnownBarrierShdata) * mu_get_system_size();
    fprintf(stderr, "shmem_barrier version %d, size: %lu, known_size: %lu\n",
            version, size, known_size);
    assert( version == KNOWN_BARRIER_VERSION );
    assert( size == known_size );
}

static void check_cpuinfo_version(void) {
    enum { KNOWN_CPUINFO_VERSION = 4 };
    enum { KNOWN_QUEUE_PROC_REQS_SIZE = 4096 };
    enum { KNOWN_QUEUE_PIDS_SIZE = 8 };
    enum { KNOWN_NUM_STATS = 3 };
    struct KnownProcRequest {
        pid_t        pid;
        unsigned int int1;
        cpu_set_t    mask;
    };
    struct KnownQueueProcs {
        struct KnownProcRequest queue[KNOWN_QUEUE_PROC_REQS_SIZE];
        unsigned int int1;
        unsigned int int2;
    };
    struct KnownPidsQueue {
        pid_t queue[KNOWN_QUEUE_PIDS_SIZE];
        unsigned int int1;
        unsigned int int2;
    };
    struct KnownCpuinfo {
        int int1;
        pid_t pid1;
        pid_t pid2;
        int int2;
        bool bool1;
        enum {ENUM1} enum1;
        enum {ENUM2} enum2;
        int64_t int3[KNOWN_NUM_STATS];
        struct timespec time1;
        struct KnownPidsQueue queue;
    };
    struct KnownCpuinfoShdata {
        struct timespec time1;
        int64_t int1;
        bool bool2;
        struct KnownQueueProcs queue;
        struct KnownCpuinfo info[0];
    };

    int version = shmem_cpuinfo__version();
    size_t size = shmem_cpuinfo__size();
    size_t known_size = sizeof(struct KnownCpuinfoShdata)
        + sizeof(struct KnownCpuinfo)*mu_get_system_size();
    fprintf(stderr, "shmem_cpuinfo version %d, size: %lu, known_size: %lu\n",
            version, size, known_size);
    assert( version == KNOWN_CPUINFO_VERSION );
    assert( size == known_size );
}

static void check_procinfo_version(void) {
    enum { KNOWN_PROCINFO_VERSION = 2 };
    struct KnownProcinfo {
        pid_t pid;
        bool bool1;
        cpu_set_t mask1;
        cpu_set_t mask2;
        cpu_set_t mask3;
        unsigned int int1;
        // Cpu Usage fields:
        double double1;
        double double2;
#ifdef DLB_LOAD_AVERAGE
        // Load average fields:
        float float1[3];
        struct timespec time;
#endif
    };
    struct KnownProcinfoShdata {
        bool bool1;
        bool bool2;
        struct timespec time;
        cpu_set_t mask;
        struct KnownProcinfo info[0];
    };

    int version = shmem_procinfo__version();
    size_t size = shmem_procinfo__size();
    size_t known_size = sizeof(struct KnownProcinfoShdata)
        + sizeof(struct KnownProcinfo)*mu_get_system_size();
    fprintf(stderr, "shmem_procinfo version %d, size: %lu, known_size: %lu\n",
            version, size, known_size);
    assert( version == KNOWN_PROCINFO_VERSION );
    assert( size == known_size );
}

int main(int argc, char **argv) {
    check_async_version();
    check_barrier_version();
    check_cpuinfo_version();
    check_procinfo_version();

    return 0;
}
