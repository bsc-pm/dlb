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

__attribute__((constructor)) static void delete_shm(void) {
    char shm_filename[SHMEM_MAX_NAME_LENGTH];
    snprintf(shm_filename, SHMEM_MAX_NAME_LENGTH, "/dev/shm/DLB_cpuinfo_%d", getuid());
    unlink(shm_filename);
    snprintf(shm_filename, SHMEM_MAX_NAME_LENGTH, "/dev/shm/DLB_procinfo_%d", getuid());
    unlink(shm_filename);
    snprintf(shm_filename, SHMEM_MAX_NAME_LENGTH, "/dev/shm/DLB_async_%d", getuid());
    unlink(shm_filename);
}

__attribute__((destructor)) static void assert_noshm(void) {
    char shm_filename[SHMEM_MAX_NAME_LENGTH];
    snprintf(shm_filename, SHMEM_MAX_NAME_LENGTH, "/dev/shm/DLB_cpuinfo_%d", getuid());
    assert( access(shm_filename, F_OK) == -1 );
    snprintf(shm_filename, SHMEM_MAX_NAME_LENGTH, "/dev/shm/DLB_procinfo_%d", getuid());
    assert( access(shm_filename, F_OK) == -1 );
    snprintf(shm_filename, SHMEM_MAX_NAME_LENGTH, "/dev/shm/DLB_async_%d", getuid());
    assert( access(shm_filename, F_OK) == -1 );
}

#endif /* ASSERT_NOSHM_H */
