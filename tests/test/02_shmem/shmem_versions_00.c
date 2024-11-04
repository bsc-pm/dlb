/*********************************************************************************/
/*  Copyright 2009-2024 Barcelona Supercomputing Center                          */
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

#include "apis/dlb_talp.h"
#include "LB_comm/shmem.h"
#include "LB_comm/shmem_async.h"
#include "LB_comm/shmem_barrier.h"
#include "LB_comm/shmem_cpuinfo.h"
#include "LB_comm/shmem_lewi_async.h"
#include "LB_comm/shmem_procinfo.h"
#include "LB_comm/shmem_talp.h"
#include "support/mask_utils.h"
#include "support/atomic.h"

#include <sched.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>

/* queue_pid_t */
#define QUEUE_T pid_t
#define QUEUE_SIZE 8
#include "support/queue_template.h"

/* queue_lewi_mask_request_t */
typedef struct {
    pid_t        pid;
    unsigned int howmany;
    cpu_set_t    allowed;
} lewi_mask_request_t;
#define QUEUE_T lewi_mask_request_t
#define QUEUE_KEY_T pid_t
#define QUEUE_SIZE 1024
#include "support/queue_template.h"

// Check versions of all shmems


static void check_shmem_sync_version(void) {
    enum { KNOWN_SHMEM_SYNC_VERSION = 3 };
    struct KnownShmemSync {
        unsigned int        uint1;
        unsigned int        uint2;
        int                 int1;
        int                 int2;
        enum {ENUM1}        enum1;
        pthread_mutex_t     mutex;
        pid_t               pidlist[];
    };

    int version = shmem_shsync__version();
    size_t size = shmem_shsync__size();
    size_t known_size = sizeof(struct KnownShmemSync) + sizeof(pid_t) * mu_get_system_size();
    size_t alignment = DLB_CACHE_LINE; // in bytes
    known_size = (known_size + (alignment - 1)) & ~(alignment - 1); // round up
    fprintf(stderr, "shmem_sync version %d, size: %zu, known_size: %zu\n",
            version, size, known_size);
    assert( version == KNOWN_SHMEM_SYNC_VERSION );
    assert( size == known_size );
}

static void check_async_version(void) {
    enum { KNOWN_ASYNC_VERSION = 4 };
    enum { KNOWN_QUEUE_SIZE = 100 };
    struct KnownMessage {
        enum {ENUM1} enum1;
        int int1;
        int int2;
        cpu_set_t cpu_set;
    };
    enum KnownStatus { status1, status2, status3 };
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
        enum KnownStatus status;
    };

    int version = shmem_async__version();
    size_t size = shmem_async__size();
    size_t known_size = sizeof(struct KnownAsyncShdata) * mu_get_system_size();
    fprintf(stderr, "shmem_async version %d, size: %zu, known_size: %zu\n",
            version, size, known_size);
    assert( version == KNOWN_ASYNC_VERSION );
    assert( size == known_size );
}

static void check_barrier_version(void) {
    enum { KNOWN_BARRIER_VERSION = 6 };
    enum { KNOWN_BARRIER_NAME_MAX = 32 };
    struct KnownBarrierFlags {
        bool flag1:1;
        bool flag2:1;
    };
    struct KnownBarrierShdata {
        char char1[KNOWN_BARRIER_NAME_MAX];
        struct KnownBarrierFlags flags;
        unsigned int int2;
        atomic_uint  int3;
        atomic_uint  int4;
        pthread_barrier_t barrier;
        pthread_rwlock_t rwlock;
    };

    int version = shmem_barrier__version();
    size_t size = shmem_barrier__size();
    size_t known_size = sizeof(struct KnownBarrierShdata) * mu_get_system_size();
    fprintf(stderr, "shmem_barrier version %d, size: %zu, known_size: %zu\n",
            version, size, known_size);
    assert( version == KNOWN_BARRIER_VERSION );
    assert( size == known_size );
}

static void check_cpuinfo_version(void) {
    enum { KNOWN_CPUINFO_VERSION = 6 };
    enum { KNOWN_QUEUE_PROC_REQS_SIZE = 4096 };
    enum { KNOWN_QUEUE_PIDS_SIZE = 8 };
    struct KnownCpuinfo {
        int int1;
        pid_t pid1;
        pid_t pid2;
        enum {ENUM1} enum1;
        queue_pid_t queue;
    };
    struct KnownCpuinfoFlags {
        bool flag1:1;
        bool flag2:1;
        bool flag3:1;
    };
    struct KnownCpuinfoShdata {
        struct KnownCpuinfoFlags flags;
        struct timespec time1;
        atomic_int_least64_t int1;
        queue_lewi_mask_request_t queue;
        cpu_set_t mask1;
        cpu_set_t mask2;
        struct KnownCpuinfo info[];
    };

    int version = shmem_cpuinfo__version();
    size_t size = shmem_cpuinfo__size();
    size_t known_size = sizeof(struct KnownCpuinfoShdata)
        + sizeof(struct KnownCpuinfo)*mu_get_system_size();
    fprintf(stderr, "shmem_cpuinfo version %d, size: %zu, known_size: %zu\n",
            version, size, known_size);
    assert( version == KNOWN_CPUINFO_VERSION );
    assert( size == known_size );
}

static void check_lewi_async_version(void) {
    enum {KNOWN_LEWI_ASYNC_VERSION = 2 };

    struct DLB_ALIGN_CACHE KnownLewiProcess {
        pid_t pid;
        unsigned int uint1;
        unsigned int uint2;
    };

    struct KnownLewiAsyncShdata {
        int int1;
        int int2;
        queue_lewi_reqs_t reqs;
        struct KnownLewiProcess processes[];
    };

    int version = shmem_lewi_async__version();
    size_t size = shmem_lewi_async__size();
    size_t known_size = sizeof(struct KnownLewiAsyncShdata)
        + sizeof(struct KnownLewiProcess)*mu_get_system_size();
    fprintf(stderr, "shmem_lewi_async version %d, size: %zu, known_size: %zu\n",
            version, size, known_size);
    assert( version == KNOWN_LEWI_ASYNC_VERSION );
    assert( size == known_size );
}

static void check_procinfo_version(void) {
    enum { KNOWN_PROCINFO_VERSION = 9 };

    struct DLB_ALIGN_CACHE KnownProcinfo {
        pid_t pid;
        bool bool1;
        bool bool2;
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
    struct KnownProcinfoFlags {
        bool flag1:1;
        bool flag2:1;
        bool flag3:1;
    };

    struct KnownProcinfoShdata {
        struct KnownProcinfoFlags flags;
        struct timespec time;
        cpu_set_t mask1;
        struct KnownProcinfo info[];
    };

    int version = shmem_procinfo__version();
    size_t size = shmem_procinfo__size();
    size_t known_size = sizeof(struct KnownProcinfoShdata)
        + sizeof(struct KnownProcinfo)*mu_get_system_size();
    fprintf(stderr, "shmem_procinfo version %d, size: %zu, known_size: %zu\n",
            version, size, known_size);
    assert( version == KNOWN_PROCINFO_VERSION );
    assert( size == known_size );
}

static void check_talp_version(void) {
    enum { KNOWN_TALP_VERSION = 3 };
    enum { KNOWN_DEFAULT_REGIONS_PER_PROC = 100 };

    struct DLB_ALIGN_CACHE TalpRegion {
        char name[DLB_MONITOR_NAME_MAX];
        atomic_int_least64_t int1;
        atomic_int_least64_t int2;
        pid_t pid;
        float float1;
    };

    struct KnownTalpShdata {
        bool bool1;
        int int1;
        struct TalpRegion talp_region[];
    };

    int version = shmem_talp__version();
    size_t size = shmem_talp__size();
    size_t known_size = sizeof(struct KnownTalpShdata)
        + sizeof(struct TalpRegion)*mu_get_system_size()*KNOWN_DEFAULT_REGIONS_PER_PROC;
    fprintf(stderr, "shmem_talp version %d, size: %zu, known_size: %zu\n",
            version, size, known_size);
    assert( version == KNOWN_TALP_VERSION );
    assert( size == known_size );
}

int main(int argc, char **argv) {
    check_shmem_sync_version();
    check_async_version();
    check_barrier_version();
    check_cpuinfo_version();
    check_lewi_async_version();
    check_procinfo_version();
    check_talp_version();

    return 0;
}
