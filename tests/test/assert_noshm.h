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
