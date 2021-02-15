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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef MPI_LIB

#include "LB_MPI/process_MPI.h"

#include "LB_MPI/DPD.h"
#include "LB_MPI/MPI_calls_coded.h"
#include "LB_core/DLB_kernel.h"
#include "LB_core/spd.h"
#include "apis/dlb.h"
#include "support/tracing.h"
#include "support/options.h"
#include "support/debug.h"
#include "support/types.h"
#include "LB_core/DLB_talp.h"
#include <mpi.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>

// MPI Globals
int _mpi_rank = -1;
int _mpi_size = -1;
int _mpis_per_node = -1;
int _node_id = -1;
int _num_nodes = -1;
int _process_id = -1;

static int use_dpd = 0;
static int init_from_mpi = 0;
static int mpi_ready = 0;
static int is_iter = 0;
static mpi_set_t lewi_mpi_calls = MPISET_ALL;
static MPI_Comm mpi_comm_node; /* MPI Communicator specific to the node */

void before_init(void) {
}

void after_init(void) {
    MPI_Comm_rank( MPI_COMM_WORLD, &_mpi_rank );
    MPI_Comm_size( MPI_COMM_WORLD, &_mpi_size );

    char hostname[HOST_NAME_MAX] = {'\0'};
    char (*recvData)[HOST_NAME_MAX] = malloc(_mpi_size * sizeof(char[HOST_NAME_MAX]));

    if (gethostname(hostname, HOST_NAME_MAX)<0) {
        perror("gethostname");
    }

    MPI_Comm_set_errhandler(MPI_COMM_WORLD, MPI_ERRORS_RETURN);
    int error_code = PMPI_Allgather (hostname, HOST_NAME_MAX, MPI_CHAR, recvData, HOST_NAME_MAX, MPI_CHAR, MPI_COMM_WORLD);

    if (error_code != MPI_SUCCESS) {
        char error_string[BUFSIZ];
        int length_of_error_string;

        MPI_Error_string(error_code, error_string, &length_of_error_string);
        fatal( "%3d: %s", _mpi_rank, error_string );
    }

    int i;
    _mpis_per_node = 0;
    for ( i=0; i<_mpi_size; i++ ) {
        if ( strcmp ( recvData[i], hostname ) == 0 ) {
            _mpis_per_node++;
        }
    }

    int (*procsIds)[2] = malloc(_mpi_size * sizeof(int[2]));
    if (_mpi_rank==0) {
        int j, maxSetNode;
        // Ceiling division (total_size/node_size)
        int nodes=(_mpi_size + _mpis_per_node - 1) /_mpis_per_node;
        int *procsPerNode = malloc(nodes * sizeof(int));
        char (*nodesIds)[HOST_NAME_MAX] = malloc(nodes * sizeof(char[HOST_NAME_MAX]));

        maxSetNode=0;
        for (i=0; i<nodes; i++) {
            memset(nodesIds[i], 0, HOST_NAME_MAX);
            procsPerNode[i]=0;
        }

        strcpy(nodesIds[0],recvData[0]);
        procsPerNode[0]=1;
        procsIds[0][0]=0;
        procsIds[0][1]=0;
        maxSetNode++;

        for(i=1; i<_mpi_size; i++) {
            j=0;
            while((strcmp(recvData[i],nodesIds[j]))&&(j<nodes)) {
                j++;
            }

            if(j>=nodes) {
                strcpy(nodesIds[maxSetNode],recvData[i]);
                procsIds[i][0]=procsPerNode[maxSetNode];
                procsIds[i][1]=maxSetNode;
                procsPerNode[maxSetNode]++;
                maxSetNode++;
            } else {
                strcpy(nodesIds[j],recvData[i]);
                procsIds[i][0]=procsPerNode[j];
                procsIds[i][1]=j;
                procsPerNode[j]++;
            }
        }
        _num_nodes  = nodes;
        free(nodesIds);
        free(procsPerNode);
    }

    free(recvData);

    int data[2];
    PMPI_Scatter(procsIds, 2, MPI_INT, data, 2, MPI_INT, 0, MPI_COMM_WORLD);
    free(procsIds);
    _process_id = data[0];
    _node_id    = data[1];

    /********************************************
     * _node_id    = _mpi_rank / _mpis_per_node;
     * _process_id = _mpi_rank % _mpis_per_node;
     ********************************************/

    // Color = node, key is 0 because we don't mind the internal rank
    //Commented code is just for Alya
//    if (_mpi_rank==0) {
//        MPI_Comm_split( MPI_COMM_WORLD, -1, 0, &mpi_comm_node );
//    }else{
        MPI_Comm_split( MPI_COMM_WORLD, _node_id, 0, &mpi_comm_node );
//    }

    if (DLB_Init(0, NULL, NULL) == DLB_SUCCESS) {
        init_from_mpi = 1;
    } else {
        // Re-initialize debug again to possibly update the verbose format string
        debug_init(&thread_spd->options);
    }

    // Policies that used dpd have been temporarily disabled
    //use_dpd = (policy == POLICY_RAL || policy == POLICY_WEIGHT || policy == POLICY_JUST_PROF);
    talp_mpi_init();
    use_dpd = 0;
    lewi_mpi_calls = thread_spd->options.lewi_mpi_calls;

    mpi_ready = 1;
}

void before_mpi(mpi_call call_type, intptr_t buf, intptr_t dest) {
    int valor_dpd;
    if(mpi_ready) {
        instrument_event(RUNTIME_EVENT, EVENT_INTO_MPI, EVENT_BEGIN);

        IntoCommunication();

        if(use_dpd) {
            unsigned long value = (unsigned long)((((buf>>5)^dest)<<5)|call_type);
            unsigned ear_size, ear_level;
             valor_dpd=dynais(value,&ear_size,&ear_level);
            //Only update if already treated previous iteration
            if( valor_dpd==-1) add_event(LOOP_STATE,5);
            else add_event(LOOP_STATE,valor_dpd);
        }

        if ((lewi_mpi_calls == MPISET_ALL && is_blocking(call_type)) ||
                (lewi_mpi_calls == MPISET_BARRIER && call_type==Barrier) ||
                (lewi_mpi_calls == MPISET_COLLECTIVES && is_collective(call_type))) {
            IntoBlockingCall(is_iter, 0);
        }

        instrument_event(RUNTIME_EVENT, EVENT_INTO_MPI, EVENT_END);
    }
}

void after_mpi(mpi_call call_type) {
    if (mpi_ready) {
        instrument_event(RUNTIME_EVENT, EVENT_OUTOF_MPI, EVENT_BEGIN);

        if ((lewi_mpi_calls == MPISET_ALL && is_blocking(call_type)) ||
                (lewi_mpi_calls == MPISET_BARRIER && call_type==Barrier) ||
                (lewi_mpi_calls == MPISET_COLLECTIVES && is_collective(call_type))) {
            OutOfBlockingCall(is_iter);
        }

        OutOfCommunication();

        instrument_event(RUNTIME_EVENT, EVENT_OUTOF_MPI, EVENT_END);
    }
    // Poll DROM and update mask if necessary
    DLB_PollDROM_Update();
}

void before_finalize(void) {
    mpi_ready = 0;
    talp_mpi_finalize();
}

void after_finalize(void) {
    if (init_from_mpi == 1) {
        init_from_mpi = 0;
        DLB_Finalize();
    }
}

int is_mpi_ready(void) {
    return mpi_ready;
}

MPI_Comm getNodeComm(void) {
    return mpi_comm_node;
}

#endif /* MPI_LIB */
