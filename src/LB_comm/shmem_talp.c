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

#include "LB_comm/shmem_talp.h"

#include "LB_comm/shmem.h"
#include "apis/dlb_errors.h"
#include "apis/dlb_talp.h"
#include "support/atomic.h"
#include "support/debug.h"
#include "support/types.h"
#include "support/mask_utils.h"

#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <inttypes.h>


enum { NOBODY = 0 };
enum { DEFAULT_REGIONS_PER_PROC = 10 };


typedef struct DLB_ALIGN_CACHE talp_region_t {
    char name[DLB_MONITOR_NAME_MAX];
    atomic_int_least64_t mpi_time;
    atomic_int_least64_t useful_time;
    pid_t pid;
} talp_region_t;

typedef struct {
    bool initialized;
    int max_regions;
    talp_region_t talp_region[];
} shdata_t;

enum { SHMEM_TALP_VERSION = 1 };

static shmem_handler_t *shm_handler = NULL;
static shdata_t *shdata = NULL;
static int regions_per_proc_initialized = 0;
static const char *shmem_name = "talp";
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static int subprocesses_attached = 0;


/*********************************************************************************/
/*  Init / Finalize                                                              */
/*********************************************************************************/

/* This function may be called from shmem_init to cleanup pid */
static void cleanup_shmem(void *shdata_ptr, int pid) {
    bool shmem_empty = true;
    shdata_t *shared_data = shdata_ptr;
    int region_id;
    int max_regions = shared_data->max_regions;
    for (region_id = 0; region_id < max_regions; ++region_id) {
        talp_region_t *talp_region = &shared_data->talp_region[region_id];
        if (talp_region->pid == pid) {
            *talp_region = (const talp_region_t){};
        } else if (talp_region->pid > 0) {
            shmem_empty = false;
        }
    }

    /* If there are no registered processes, make sure shmem is reset */
    if (shmem_empty) {
        memset(shared_data, 0, shmem_talp__size());
    }
}

static bool is_shmem_empty(void) {
    int region_id;
    int max_regions = shdata->max_regions;
    for (region_id = 0; region_id < max_regions; ++region_id) {
        if (shdata->talp_region[region_id].pid != NOBODY) {
            return false;
        }
    }
    return true;
}

static void open_shmem(const char *shmem_key, int regions_per_process) {
    pthread_mutex_lock(&mutex);
    {
        if (shm_handler == NULL) {
            // Use either requested or default value
            regions_per_proc_initialized = regions_per_process ?
                regions_per_process : DEFAULT_REGIONS_PER_PROC;

            shm_handler = shmem_init((void**)&shdata,
                    &(const shmem_props_t) {
                        .size = shmem_talp__size(),
                        .name = shmem_name,
                        .key = shmem_key,
                        .version = SHMEM_TALP_VERSION,
                        .cleanup_fn = cleanup_shmem,
                    });
            subprocesses_attached = 1;
        } else {
            ++subprocesses_attached;
        }
    }
    pthread_mutex_unlock(&mutex);
}

static void close_shmem(void) {
    pthread_mutex_lock(&mutex);
    {
        if (--subprocesses_attached == 0) {
            shmem_finalize(shm_handler, is_shmem_empty);
            shm_handler = NULL;
            shdata = NULL;
        }
    }
    pthread_mutex_unlock(&mutex);
}

int shmem_talp__init(const char *shmem_key, int regions_per_process) {
    int error = DLB_SUCCESS;

    // Shared memory creation
    open_shmem(shmem_key, regions_per_process);

    shmem_lock(shm_handler);
    {
        // Initialize some values if this is the 1st process attached to the shmem
        if (!shdata->initialized) {
            shdata->initialized = true;
            shdata->max_regions = mu_get_system_size() * (regions_per_process ?
                    regions_per_process : DEFAULT_REGIONS_PER_PROC);
        } else {
            if (regions_per_process != 0
                    && shdata->max_regions != regions_per_process * mu_get_system_size()) {
                error = DLB_ERR_INIT;
            }
        }
    }
    shmem_unlock(shm_handler);

    if (error == DLB_ERR_INIT) {
        warning("Cannot initialize TALP shmem for %d regions per process, already"
                " allocated for %d, accounting for %d processes. Check for DLB_ARGS"
                " consistency among processes or clean up shared memory.",
                regions_per_process, shdata->max_regions, mu_get_system_size());
    }

    if (error < DLB_SUCCESS) {
        // The shared memory contents are untouched, but the counter needs to
        // be decremented, and the shared memory deleted if needed
        close_shmem();
    }

    return error;
}

int shmem_talp_ext__init(const char *shmem_key, int regions_per_process) {
    // Shared memory creation
    open_shmem(shmem_key, regions_per_process);

    // External processes don't need to initialize anything

    return DLB_SUCCESS;
}

int shmem_talp__finalize(pid_t pid) {
    int error = DLB_SUCCESS;

    if (shm_handler == NULL) {
        return DLB_ERR_NOSHMEM;
    }

    // Remove all regions associated with pid
    shmem_lock(shm_handler);
    {
        int region_id;
        int max_regions = shdata->max_regions;
        for (region_id = 0; region_id < max_regions; ++region_id) {
            talp_region_t *talp_region = &shdata->talp_region[region_id];
            if (talp_region->pid == pid) {
                *talp_region = (const talp_region_t){};
            }
        }
    }
    shmem_unlock(shm_handler);

    // Close shared memory
    close_shmem();

    return error;
}

int shmem_talp_ext__finalize(void) {
    // Protect double finalization
    if (shm_handler == NULL) {
        return DLB_ERR_NOSHMEM;
    }

    // Shared memory destruction
    close_shmem();

    return DLB_SUCCESS;
}


/*********************************************************************************/
/*  Register regions                                                             */
/*********************************************************************************/

/* Register monitoring region with name "name", or look up if already registered.
 * Return associated node-unique id by parameter. */
int shmem_talp__register(pid_t pid, const char *name, int *node_shared_id) {
    if (shm_handler == NULL) return DLB_ERR_NOSHMEM;

    int error;
    shmem_lock(shm_handler);
    {
        /* Iterate until region is found, find an empty spot in the same pass */
        talp_region_t *empty_spot = NULL;
        int found_id = -1;
        int region_id;
        int max_regions = shdata->max_regions;
        for (region_id = 0; region_id < max_regions; ++region_id) {
            talp_region_t *talp_region = &shdata->talp_region[region_id];
            if (empty_spot == NULL
                    && talp_region->pid == NOBODY) {
                /* First empty spot */
                empty_spot = talp_region;
                found_id = region_id;
            }
            else if (talp_region->pid == pid
                    && strncmp(talp_region->name, name, DLB_MONITOR_NAME_MAX-1) == 0) {
                /* Region was already registered */
                found_id = region_id;
                break;
            }
        }

        if (found_id == region_id) {
            /* Already registered */
            *node_shared_id = found_id;
            error = DLB_NOUPDT;
        } else if (region_id == max_regions && empty_spot != NULL) {
            /* Register new region */
            *empty_spot = (const talp_region_t) {.pid = pid};
            snprintf(empty_spot->name, DLB_MONITOR_NAME_MAX, "%s", name);
            *node_shared_id = found_id;
            error = DLB_SUCCESS;
        } else {
            /* No mem left */
            error = DLB_ERR_NOMEM;
        }
    }
    shmem_unlock(shm_handler);

    return error;
}

/*********************************************************************************/
/*  Getters                                                                      */
/*********************************************************************************/

/* Obtain a list of PIDs that have registered a region */
int shmem_talp__get_pidlist(pid_t *pidlist, int *nelems, int max_len) {
    if (shm_handler == NULL) return DLB_ERR_NOSHMEM;

    *nelems = 0;
    shmem_lock(shm_handler);
    {
        int region_id;
        int max_regions = shdata->max_regions;
        for (region_id = 0; region_id < max_regions && *nelems < max_len; ++region_id) {
            pid_t pid = shdata->talp_region[region_id].pid;
            if (pid != NOBODY) {
                // pid is valid, check if already in pidlist
                int i;
                for (i=0; i<(*nelems); ++i) {
                    if (pid == pidlist[i])
                        break;
                }
                // add if not found
                if (i == *nelems) {
                    pidlist[(*nelems)++] = pid;
                }
            }
        }
    }
    shmem_unlock(shm_handler);
    return DLB_SUCCESS;
}

static int cmp_region_list(const void *elem1, const void *elem2) {
    return ((talp_region_list_t*)elem1)->pid - ((talp_region_list_t*)elem2)->pid;
}

/* Look up for a registered monitoring region with the given "name" and "pid". */
int shmem_talp__get_region(talp_region_list_t *region, pid_t pid, const char *name) {
    if (shm_handler == NULL) return DLB_ERR_NOSHMEM;

    int error = DLB_ERR_NOPROC;
    shmem_lock(shm_handler);
    {
        int region_id;
        int max_regions = shdata->max_regions;
        for (region_id = 0; region_id < max_regions; ++region_id) {
            talp_region_t *talp_region = &shdata->talp_region[region_id];
            if (talp_region->pid == pid
                    && strncmp(talp_region->name, name, DLB_MONITOR_NAME_MAX-1) == 0) {
                *region = (const talp_region_list_t) {
                    .pid = pid,
                    .region_id = region_id,
                    .mpi_time = DLB_ATOMIC_LD_RLX(&talp_region->mpi_time),
                    .useful_time = DLB_ATOMIC_LD_RLX(&talp_region->useful_time),
                };
                error = DLB_SUCCESS;
                break;
            }
        }
    }
    shmem_unlock(shm_handler);

    return error;
}

/* Obtain a list of regions for a given name, sorted by PID */
int shmem_talp__get_regionlist(talp_region_list_t *region_list, int *nelems,
        int max_len, const char *name) {
    if (shm_handler == NULL) return DLB_ERR_NOSHMEM;

    *nelems = 0;
    shmem_lock(shm_handler);
    {
        int region_id;
        int max_regions = shdata->max_regions;
        for (region_id = 0; region_id < max_regions && *nelems < max_len; ++region_id) {
            talp_region_t *talp_region = &shdata->talp_region[region_id];
            if (talp_region->pid != NOBODY
                    && strncmp(talp_region->name, name, DLB_MONITOR_NAME_MAX-1) == 0) {
                region_list[(*nelems)++] = (const talp_region_list_t) {
                    .pid = talp_region->pid,
                    .region_id = region_id,
                    .mpi_time = DLB_ATOMIC_LD_RLX(&talp_region->mpi_time),
                    .useful_time = DLB_ATOMIC_LD_RLX(&talp_region->useful_time),
                };
            }
        }
    }
    shmem_unlock(shm_handler);

    /* Sort array by PID */
    qsort(region_list, *nelems, sizeof(talp_region_list_t), cmp_region_list);

    return DLB_SUCCESS;
}

int shmem_talp__get_times(int region_id, int64_t *mpi_time, int64_t *useful_time) {
    if (unlikely(shm_handler == NULL)) return DLB_ERR_NOSHMEM;
    if (unlikely(region_id >= shdata->max_regions)) return DLB_ERR_NOMEM;
    if (unlikely(region_id < 0)) return DLB_ERR_NOENT;

    talp_region_t *talp_region = &shdata->talp_region[region_id];
    if (unlikely(talp_region->pid == NOBODY)) return DLB_ERR_NOENT;

    *mpi_time    = DLB_ATOMIC_LD_RLX(&talp_region->mpi_time);
    *useful_time = DLB_ATOMIC_LD_RLX(&talp_region->useful_time);

    return DLB_SUCCESS;
}


/*********************************************************************************/
/*  Setters                                                                      */
/*********************************************************************************/

int shmem_talp__set_times(int region_id, int64_t mpi_time, int64_t useful_time) {
    if (unlikely(shm_handler == NULL)) return DLB_ERR_NOSHMEM;
    if (unlikely(region_id >= shdata->max_regions)) return DLB_ERR_NOMEM;
    if (unlikely(region_id < 0)) return DLB_ERR_NOENT;

    talp_region_t *talp_region = &shdata->talp_region[region_id];
    if (unlikely(talp_region->pid == NOBODY)) return DLB_ERR_NOENT;

    DLB_ATOMIC_ST_RLX(&talp_region->mpi_time, mpi_time);
    DLB_ATOMIC_ST_RLX(&talp_region->useful_time, useful_time);

    return DLB_SUCCESS;
}


/*********************************************************************************/
/* Misc                                                                          */
/*********************************************************************************/

void shmem_talp__print_info(const char *shmem_key, int regions_per_process) {

    /* If the shmem is not opened, obtain a temporary fd */
    bool temporary_shmem = shm_handler == NULL;
    if (temporary_shmem) {
        shmem_talp_ext__init(shmem_key, regions_per_process);
    }

    /* Make a full copy of the shared memory */
    shdata_t *shdata_copy = malloc(shmem_talp__size());
    shmem_lock(shm_handler);
    {
        memcpy(shdata_copy, shdata, shmem_talp__size());
    }
    shmem_unlock(shm_handler);

    /* Close shmem if needed */
    if (temporary_shmem) {
        shmem_talp_ext__finalize();
    }

    /* Find the max number of characters per column */
    pid_t max_pid = 111;    /* 3 digits for 'PID' */
    int max_name = 4;       /* 'Name' */
    int max_mpi = 8;        /* 'MPI time' */
    int max_useful = 11;    /* 'Useful time' */
    int region_id;
    int max_regions = shdata_copy->max_regions;
    for (region_id = 0; region_id < max_regions; ++region_id) {
        talp_region_t *talp_region = &shdata_copy->talp_region[region_id];
        if (talp_region->pid != NOBODY) {
            int len;
            /* PID */
            max_pid = max_int(talp_region->pid, max_pid);
            /* Name */
            len = strlen(talp_region->name);
            max_name = max_int(len, max_name);
            /* MPI time */
            len = snprintf(NULL, 0, "%"PRId64, talp_region->mpi_time);
            max_mpi = max_int(len, max_mpi);
            /* Useful time */
            len = snprintf(NULL, 0, "%"PRId64, talp_region->useful_time);
            max_useful = max_int(len, max_useful);
        }
    }
    int max_pid_digits = snprintf(NULL, 0, "%d", max_pid);

    /* Initialize buffer */
    print_buffer_t buffer;
    printbuffer_init(&buffer);

    /* Set up line buffer */
    enum { MAX_LINE_LEN = 512 };
    char line[MAX_LINE_LEN];

    for (region_id = 0; region_id < max_regions; ++region_id) {
        talp_region_t *talp_region = &shdata_copy->talp_region[region_id];
        if (talp_region->pid != NOBODY) {

            /* Append line to buffer */
            snprintf(line, MAX_LINE_LEN,
                    "  | %*d | %*s | %*"PRId64" | %*"PRId64" |",
                    max_pid_digits, talp_region->pid,
                    max_name, talp_region->name,
                    max_mpi, talp_region->mpi_time,
                    max_useful, talp_region->useful_time),
            printbuffer_append(&buffer, line);
        }
    }

    /* Print if not empty */
    if (buffer.addr[0] != '\0' ) {
        /* Construct header */
        snprintf(line, MAX_LINE_LEN,
                "  | %*s | %*s | %*s | %*s |",
                max_pid_digits, "PID",
                max_name, "Name",
                max_mpi, "MPI time",
                max_useful, "Useful time");

        /* Print header + buffer */
        info0("=== TALP Regions ===\n"
              "%s\n"
              "%s", line, buffer.addr);
    }

    printbuffer_destroy(&buffer);
    free(shdata_copy);
}

bool shmem_talp__exists(void) {
    return shm_handler != NULL;
}

bool shmem_talp__initialized(void) {
    return shdata && shdata->initialized;
}

int shmem_talp__version(void) {
    return SHMEM_TALP_VERSION;
}

size_t shmem_talp__size(void) {
    int regions_per_process =
        regions_per_proc_initialized ? regions_per_proc_initialized : DEFAULT_REGIONS_PER_PROC;
    int num_regions = mu_get_system_size() * regions_per_process;
    return sizeof(shdata_t) + sizeof(talp_region_t)*num_regions;
}

int shmem_talp__get_max_regions(void) {
    return shdata ? shdata->max_regions : 0;
}
