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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "LB_core/node_barrier.h"

#include "apis/dlb_errors.h"
#include "apis/dlb_types.h"
#include "LB_core/spd.h"
#include "LB_comm/shmem_barrier.h"
#include "support/debug.h"
#include "support/tracing.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

/* Per process, private Barrier data */
typedef struct barrier_info {
    char default_barrier_name[BARRIER_NAME_MAX]; /* only to keep compatibility with --barrier-id */
    barrier_t *default_barrier;
    barrier_t **barrier_list;
    int max_barriers;
} barrier_info_t;

static const char *default_barrier_name = "default";
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/* Parse, for the specific barrier, whether it should do LeWI based on:
 * - if barrier_name == default_barrier_name:
 *      if lewi_barrier and !lewi_barrier_select;
 *      or "default" in lewi_barrier_select
 * - else:
 *      if barrier_flags has LEWI;
 *      or if barrier_flags has SELECTIVE and name in lewi_barrier_select
 */
static bool parse_lewi_barrier(const char *barrier_name, bool lewi_barrier,
        const char *lewi_barrier_select, int api_flags) {
    if (strncmp(barrier_name, default_barrier_name, BARRIER_NAME_MAX) == 0) {
        if (strlen(lewi_barrier_select) == 0) {
            /* Default barrier: --lewi-barrier-select not set, --lewi-barrier dictates */
            return lewi_barrier;
        }
    } else {
        if (api_flags == DLB_BARRIER_LEWI_ON) {
            /* Named barrier: LeWI is forced by API */
            return true;
        } else if (api_flags == DLB_BARRIER_LEWI_OFF) {
            /* Named barrier: LeWI is disallowed by API */
            return false;
        }
    }

    /* Find barrier_name in --lewi-barrier-select */
    size_t len = strlen(lewi_barrier_select);
    if (len > 0) {
        bool found_in_select = false;
        char *barrier_select_copy = malloc(sizeof(char)*(len+1));
        strcpy(barrier_select_copy, lewi_barrier_select);
        char *saveptr;
        char *token = strtok_r(barrier_select_copy, ",", &saveptr);
        while (token) {
            if (strcmp(token, barrier_name) == 0) {
                found_in_select = true;
                break;
            }
            /* next token */
            token = strtok_r(NULL, ",", &saveptr);
        }
        free(barrier_select_copy);

        return found_in_select;
    }

    return false;
}

void node_barrier_init(subprocess_descriptor_t *spd) {
    if (spd->barrier_info != NULL) {
        fatal("Cannot initialize Node Barrier, barrier_info not NULL\n"
                "Please, report bug at " PACKAGE_BUGREPORT);
    }

    /* Even though default_barrier_name may change, no harm to use it here
     * because we parse the user's options. */
    bool lewi_barrier = parse_lewi_barrier(default_barrier_name,
            spd->options.lewi_barrier, spd->options.lewi_barrier_select, 0);

    barrier_info_t *barrier_info;
    pthread_mutex_lock(&mutex);
    {
        /* Initialize barrier_info */
        barrier_info = malloc(sizeof(barrier_info_t));
        *barrier_info = (const barrier_info_t){};
        spd->barrier_info = barrier_info;

        /* --barrier-id may be deprecated in the future, but for now we just modify
        * the default barrier name so that processes with different barrier id's
        * don't synchronize with each other. */
        if (spd->options.barrier_id == 0) {
            sprintf(barrier_info->default_barrier_name, "%s", default_barrier_name);
        } else {
            snprintf(barrier_info->default_barrier_name, BARRIER_NAME_MAX,
                    "default (id: %d)", spd->options.barrier_id);
        }

        /* Initialize default barrier */
        barrier_info->default_barrier = shmem_barrier__register(
                barrier_info->default_barrier_name, lewi_barrier);

        /* Initialize barrier_list */
        barrier_info->max_barriers = shmem_barrier__get_max_barriers();
        barrier_info->barrier_list = calloc(barrier_info->max_barriers, sizeof(void*));
    }
    pthread_mutex_unlock(&mutex);

    if (barrier_info->default_barrier == NULL) {
        warning("DLB system barrier could nout be initialized");
    }
}

void node_barrier_finalize(subprocess_descriptor_t *spd) {
    pthread_mutex_lock(&mutex);
    {
        if (spd->barrier_info != NULL) {
            /* Detach all, no need to check for non NULL values */
            barrier_info_t *barrier_info = spd->barrier_info;
            shmem_barrier__detach(barrier_info->default_barrier);
            int i;
            for (i=0; i<barrier_info->max_barriers; ++i) {
                shmem_barrier__detach(barrier_info->barrier_list[i]);
            }
            free(barrier_info->barrier_list);
            *barrier_info = (const barrier_info_t){};
            free(spd->barrier_info);
            spd->barrier_info = NULL;
        }
    }
    pthread_mutex_unlock(&mutex);
}

barrier_t* node_barrier_register(subprocess_descriptor_t *spd,
        const char *barrier_name, int flags) {

    /* This function does not allow registering the default barrier */
    if (barrier_name == NULL) return NULL;

    barrier_t *barrier = NULL;
    if (spd->options.barrier) {
        /* The register function cannot know whether the calling process is a new
        * participant or just a query for the pointer. If we have at least one
        * registered named barrier, we need to check the shared memory first. */
        barrier_info_t *barrier_info = spd->barrier_info;
        if (barrier_info->barrier_list[0] != NULL) {
            barrier = shmem_barrier__find(barrier_name);
            if (barrier != NULL) {
                /* Barrier is found in shmem, check if it's registered within the spd. */
                int i;
                int max_barriers = barrier_info->max_barriers;
                for (i=0; i<max_barriers; ++i) {
                    if (barrier_info->barrier_list[i] == barrier) {
                        /* Barrier already registered in spd */
                        return barrier;
                    }
                }
                /* Barrier is not registered within this spd */
                barrier = NULL;
            }
        }

        /* Register if not found */
        if (barrier == NULL) {
            bool lewi_barrier = parse_lewi_barrier(barrier_name,
                    spd->options.lewi_barrier,
                    spd->options.lewi_barrier_select, flags);
            barrier = shmem_barrier__register(barrier_name, lewi_barrier);
            if (barrier == NULL) return NULL;
        }

        /* Update the barrier list, if needed */
        int i;
        int max_barriers = barrier_info->max_barriers;
        pthread_mutex_lock(&mutex);
        {
            for (i=0; i<max_barriers; ++i) {
                if (barrier_info->barrier_list[i] == NULL) {
                    barrier_info->barrier_list[i] = barrier;
                    break;
                }
            }
        }
        pthread_mutex_unlock(&mutex);

        ensure(i < max_barriers, "Cannot register Node Barrier, no space left in"
                " barrier_list.\nPlease, report bug at " PACKAGE_BUGREPORT);
    }

    return barrier;
}

int node_barrier(const subprocess_descriptor_t *spd, barrier_t *barrier) {
    int error;
    if (spd->options.barrier) {
        /* Check whether barrier is valid */
        barrier_info_t *barrier_info = spd->barrier_info;
        if (barrier == NULL) {
            /* If barrier is not provided we only need to check the reserved
             * position in barrier_list */
            if (barrier_info->default_barrier != NULL) {
                barrier = barrier_info->default_barrier;
            }
        } else if (unlikely(barrier == barrier_info->default_barrier)) {
            /* barrier provided is the default barrier, nothing to do.
             * (default_barrier pointer is never exposed, keep this one just in case) */
        } else {
            /* Otherwise, we need to check whether the provided barrier has
             * not been detached */
            int i = 0;
            int max_barriers = barrier_info->max_barriers;
            pthread_mutex_lock(&mutex);
            {
                while (i<max_barriers
                        && barrier_info->barrier_list[i] != NULL
                        && barrier_info->barrier_list[i] != barrier) {
                    ++i;
                }

                if (i == max_barriers || barrier_info->barrier_list[i] == NULL) {
                    /* Not found in barrier_list */
                    barrier = NULL;
                }
            }
            pthread_mutex_unlock(&mutex);
        }

        /* If barrier was found, perform the actual barrier */
        if (barrier != NULL) {
            instrument_event(RUNTIME_EVENT, EVENT_BARRIER, EVENT_BEGIN);
            shmem_barrier__barrier(barrier);
            instrument_event(RUNTIME_EVENT, EVENT_BARRIER, EVENT_END);
            error = DLB_SUCCESS;
        } else {
            /* barrier not found in barrier_info, possibly a detached barrier */
            error = DLB_NOUPDT;
        }
    } else {
        error = DLB_ERR_NOCOMP;
    }

    return error;
}

int node_barrier_attach(subprocess_descriptor_t *spd, barrier_t *barrier) {
    int error;
    if (spd->options.barrier) {
        barrier_info_t *barrier_info = spd->barrier_info;
        if (barrier == NULL) {
            if (barrier_info->default_barrier == NULL) {
                /* Register default barrier again */
                bool lewi_barrier = parse_lewi_barrier(default_barrier_name,
                        spd->options.lewi_barrier,
                        spd->options.lewi_barrier_select, 0);
                barrier_info->default_barrier = shmem_barrier__register(
                        barrier_info->default_barrier_name,
                        lewi_barrier);
                // return number of participants
                error = barrier_info->default_barrier ? 1 : DLB_ERR_NOMEM;
            } else {
                /* Default barrier already attached */
                error = DLB_ERR_PERM;
            }
        } else if (unlikely(barrier == barrier_info->default_barrier)) {
            /* barrier provided is the default barrier, already attached.
             * (default_barrier pointer is never exposed, keep this one just in case) */
            error = DLB_ERR_PERM;
        } else {
            int i = 0;
            int max_barriers = shmem_barrier__get_max_barriers();
            pthread_mutex_lock(&mutex);
            {
                /* Find first NULL place or barrier in barrier_list */
                while (i<max_barriers
                        && barrier_info->barrier_list[i] != NULL
                        && barrier_info->barrier_list[i] != barrier) {
                    ++i;
                }

                if (i < max_barriers && barrier_info->barrier_list[i] == barrier) {
                    /* Already in the barrier_list */
                    error = DLB_ERR_PERM;
                } else {
                    /* Attach */
                    error = shmem_barrier__attach(barrier);

                    /* Add barrier to the barrier_list */
                    if (error >= 0) {
                        barrier_info->barrier_list[i] = barrier;
                    }
                }
            }
            pthread_mutex_unlock(&mutex);
        }
    } else {
        /* no --barrier */
        error = DLB_ERR_NOCOMP;
    }

    return error;
}

int node_barrier_detach(subprocess_descriptor_t *spd, barrier_t *barrier) {
    int error;
    if (spd->options.barrier) {
        barrier_info_t *barrier_info = spd->barrier_info;
        if (barrier == NULL) {
            if (barrier_info->default_barrier != NULL) {
                /* Detach default barrier */
                error = shmem_barrier__detach(barrier_info->default_barrier);
                if (error >= 0) {
                    barrier_info->default_barrier = NULL;
                }
            } else {
                /* Default barrier already detached */
                error = DLB_ERR_PERM;
            }
        } else if (unlikely(barrier == barrier_info->default_barrier)) {
            /* Detach default barrier.
             * (default_barrier pointer is never exposed, keep this one just in case) */
            error = shmem_barrier__detach(barrier_info->default_barrier);
            if (error >= 0) {
                barrier_info->default_barrier = NULL;
            }
        } else {
            int i = 0;
            int max_barriers = shmem_barrier__get_max_barriers();
            pthread_mutex_lock(&mutex);
            {
                /* Find first NULL place or barrier in barrier_list */
                while (i<max_barriers
                        && barrier_info->barrier_list[i] != NULL
                        && barrier_info->barrier_list[i] != barrier) {
                    ++i;
                }

                if (i == max_barriers || barrier_info->barrier_list[i] == NULL) {
                    /* Not found in barrier_list */
                    error = DLB_ERR_PERM;
                } else {
                    /* Detach */
                    error = shmem_barrier__detach(barrier);

                    /* Remove barrier from the barrier_list */
                    if (error >= 0) {
                        memmove(&barrier_info->barrier_list[i],
                                &barrier_info->barrier_list[i+1],
                                sizeof(barrier_info->barrier_list[0]) * (max_barriers-1-i));
                        barrier_info->barrier_list[max_barriers-1] = NULL;
                    }
                }
            }
            pthread_mutex_unlock(&mutex);
        }
    } else {
        /* no --barrier */
        error = DLB_ERR_NOCOMP;
    }

    return error;
}
