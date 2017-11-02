/*********************************************************************************/
/*  Copyright 2017 Barcelona Supercomputing Center                               */
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
/*  along with DLB.  If not, see <http://www.gnu.org/licenses/>.                 */
/*********************************************************************************/

#include "LB_comm/comm_lend_light.h"

#include "LB_comm/shmem.h"
#include "support/tracing.h"
#include "support/debug.h"

#include <stdlib.h>

#define MIN(X, Y)  ((X) < (Y) ? (X) : (Y))

int procs;
int me;
int node;

int defaultCPUS;
int greedy;
//pointers to the shared memory structures
struct shdata {
    int   idleCpus;
};

struct shdata *shdata;
static shmem_handler_t *shm_handler = NULL;;

void ConfigShMem(int num_procs, int meId, int nodeId, int defCPUS, int is_greedy,
        const char *shmem_key) {
    verbose(VB_SHMEM, "LoadCommonConfig");
    procs=num_procs;
    me=meId;
    node=nodeId;
    defaultCPUS=defCPUS;
    greedy=is_greedy;

    shm_handler = shmem_init((void**)&shdata, sizeof(struct shdata), "lewi", shmem_key,
            SHMEM_VERSION_IGNORE);

    if (me==0) {
        verbose(VB_SHMEM, "setting values to the shared mem");

        /* idleCPUS */
        shdata->idleCpus = 0;
        add_event(IDLE_CPUS_EVENT, 0);

        verbose(VB_SHMEM, "Finished setting values to the shared mem");
    }
}

void finalize_comm() {
    if (shm_handler) {
        shmem_finalize(shm_handler, SHMEM_DELETE);
    }
}

int releaseCpus(int cpus) {
    verbose(VB_SHMEM, "Releasing CPUS...");

    __sync_fetch_and_add (&(shdata->idleCpus), cpus);
    add_event(IDLE_CPUS_EVENT, shdata->idleCpus);

    verbose(VB_SHMEM, "DONE Releasing CPUS (idle %d)", shdata->idleCpus);

    return 0;
}

/*
Returns de number of cpus
that are assigned
*/
int acquireCpus(int current_cpus) {
    verbose(VB_SHMEM, "Acquiring CPUS...");
    int cpus = defaultCPUS-current_cpus;

//I don't care if there aren't enough cpus

    if((__sync_sub_and_fetch (&(shdata->idleCpus), cpus)>0) && greedy) {
        cpus+=__sync_val_compare_and_swap(&(shdata->idleCpus), shdata->idleCpus, 0);
    }
    add_event(IDLE_CPUS_EVENT, shdata->idleCpus);

    verbose(VB_SHMEM, "Using %d CPUS... %d Idle", cpus, shdata->idleCpus);

    return cpus+current_cpus;
}

/*
Returns de number of cpus
that are assigned
*/
int checkIdleCpus(int myCpus, int maxResources) {
    verbose(VB_SHMEM, "Checking idle CPUS... %d", shdata->idleCpus);
    int cpus;
    int aux;
//WARNING//
    //if more CPUS than the availables are used release some
    if ((shdata->idleCpus < 0) && (myCpus>defaultCPUS) ) {
        aux=shdata->idleCpus;
        cpus=MIN(abs(aux), myCpus-defaultCPUS);
        if(__sync_bool_compare_and_swap(&(shdata->idleCpus), aux, aux+cpus)) {
            myCpus-=cpus;
        }

        //if there are idle CPUS use them
    } else if( shdata->idleCpus > 0) {
        aux=shdata->idleCpus;
        if(aux>maxResources) { aux=maxResources; }

        if(__sync_bool_compare_and_swap(&(shdata->idleCpus), shdata->idleCpus, shdata->idleCpus-aux)) {
            myCpus+=aux;
        }
    }
    add_event(IDLE_CPUS_EVENT, shdata->idleCpus);

    verbose(VB_SHMEM, "Using %d CPUS... %d Idle", myCpus, shdata->idleCpus);
    return myCpus;
}
