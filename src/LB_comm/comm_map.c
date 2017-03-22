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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "LB_comm/comm_map.h"

#include "LB_comm/shmem.h"
#include "support/debug.h"

#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>

#define MIN(X, Y)  ((X) < (Y) ? (X) : (Y))

int procs;
int me;
int node;

int defaultCPUS;
//pointers to the shared memory structures

/*struct info_proc{
    int tids[CPUS_NODE];
    int cpus;

};*/

struct shdata {
    int   idleCpus;
    int mutex;
    int cpus_map[CPUS_NODE+1][CPUS_NODE];
//  int tids[CPUS_NODE][CPUS_NODE];
    int num_cpus_proc[CPUS_NODE];
    int claimed_cpus_proc[CPUS_NODE];
};

struct shdata *shdata;
static shmem_handler_t *shm_handler;

cpu_set_t all_cpu;

void print_map() {
    int i,j;
    for (i=0; i<procs; i++) {
        fprintf(stderr, "\nProc %d (%d*%d): ", i, shdata->num_cpus_proc[i], shdata->claimed_cpus_proc[i]);
        j=0;
        while (j<CPUS_NODE) {
            fprintf(stderr, "%d ", shdata->cpus_map[i][j]);
            j++;
        }
    }
    j=0;
    fprintf(stderr, "\nIdle (%d) : ", shdata->idleCpus);
    while (j<CPUS_NODE) {
        fprintf(stderr, "%d ", shdata->cpus_map[procs][j]);
        j++;
    }
    fprintf(stderr, "\n");
}

void ConfigShMem_Map(int num_procs, int meId, int nodeId, int defCPUS, int *my_cpus,
        const char *shmem_key) {
    verbose(VB_SHMEM, "LoadCommonConfig");
    int i, j;
    procs=num_procs;
    me=meId;
    node=nodeId;
    defaultCPUS=defCPUS;

    CPU_ZERO(&all_cpu);//prepare cpu_set
    for(i=0; i<CPUS_NODE; i++) { CPU_SET(i, &all_cpu); }

    shm_handler = shmem_init((void**)&shdata, sizeof(struct shdata), "lewi", shmem_key);

    if (me==0) {
        verbose(VB_SHMEM, "setting values to the shared mem");
        /* idleCPUS */
        shdata->idleCpus = 0;

        for (i=0; i<=procs; i++) {
            for(j=0; j<CPUS_NODE; j++) { shdata->cpus_map[i][j]=-1; }
            shdata->claimed_cpus_proc[i]=0;
        }

        shdata->mutex=-1;

        //add_event(IDLE_CPUS_EVENT, 0);

        verbose(VB_SHMEM, "Finished setting values to the shared mem");
    }

    //Each Process initialize his cpus
    for (i=0; i<defCPUS; i++) {
//      fprintf(stderr, "%d: %d = %d\n", me, i, (me*defCPUS)+i);
        shdata->cpus_map[me][i]=(me*defCPUS)+i;
    }
    shdata->num_cpus_proc[me]=defCPUS;
    memcpy(my_cpus, &(shdata->cpus_map[me][0]), defCPUS * sizeof(int));

    //I set the master affinity
    cpu_set_t cpu_set;
    CPU_ZERO(&cpu_set);//prepare cpu_set
    CPU_SET(shdata->cpus_map[me][0], &cpu_set);
    if(sched_setaffinity(syscall(SYS_gettid), sizeof(cpu_set), &cpu_set)<0) { perror("ERROR: sched_setaffinity of master thread"); }
//add_event(1005, shdata->cpus_map[me][0]+1);
}

void finalize_comm_Map() {
    shmem_finalize(shm_handler, SHMEM_DELETE);
}

int releaseCpus_Map(int cpus, int* released_cpus) {
    int i;
    while(!__sync_bool_compare_and_swap(&(shdata->mutex), -1, me)) { sched_yield(); }
//  fprintf(stderr,"DLB DEBUG: (%d:%d) mutex: %d\n", node, me, shdata->mutex);
    //print_map();

    int I_have=shdata->num_cpus_proc[me] + shdata->claimed_cpus_proc[me];
#ifdef debugMap
    fprintf(stderr,"DLB DEBUG: (%d:%d) Releasing(%d) but already claimed (%d) ", node, me, cpus, shdata->claimed_cpus_proc[me]);
    for (i=0; i<cpus-shdata->claimed_cpus_proc[me]; i++) {
        fprintf(stderr,"%d ", shdata->cpus_map[me][i]);
    }
    fprintf(stderr,"\n");
#endif
    memcpy(released_cpus, &(shdata->cpus_map[me][I_have-cpus]), cpus * sizeof(int));
    memcpy(&(shdata->cpus_map[procs][shdata->idleCpus]), &(shdata->cpus_map[me][I_have-cpus]), (cpus-shdata->claimed_cpus_proc[me]) * sizeof(int));
//fprintf(stderr,"DLB DEBUG: (%d:%d) I_have=%d+%d=%d cpus=%d\n", node, me, shdata->num_cpus_proc[me], shdata->claimed_cpus_proc[me], I_have, cpus);

    for (i=1; i<=cpus; i++) {
        shdata->cpus_map[me][I_have-i]=-(me+1);
    }

    shdata->num_cpus_proc[me]-= cpus;

    shdata->idleCpus+= (cpus-shdata->claimed_cpus_proc[me]);
    shdata->claimed_cpus_proc[me]=0;

    //add_event(IDLE_CPUS_EVENT, shdata->idleCpus);

    //print_map();
//  fprintf(stderr,"DLB DEBUG: (%d:%d) mutex: %d\n", node, me, shdata->mutex);
#ifdef debugMap
    fprintf(stderr,"DLB DEBUG: (%d:%d) Done Releasing, Idle cpus: ", node, me);
    for (i=0; i<shdata->idleCpus; i++) {
        fprintf(stderr,"%d ", shdata->cpus_map[procs][i]);
    }
    fprintf(stderr,"\n");
#endif
    while(!__sync_bool_compare_and_swap(&(shdata->mutex), me, -1));


    //Just in case I let the thread run in all the cpus
    if(sched_setaffinity(syscall(SYS_gettid), sizeof(all_cpu), &all_cpu)<0) { perror("ERROR: sched_setaffinity of master thread"); }
//add_event(1005, 30);

    return 0;
}

/*
Returns de number of cpus
that are assigned
*/
int acquireCpus_Map(int current_cpus, int* new_cpus) {
    int i;
    verbose(VB_SHMEM, "Acquiring CPUS...");
    int cpus = defaultCPUS-current_cpus;
    int overloading =0;
    int tmp_cpus;
    while(!__sync_bool_compare_and_swap(&(shdata->mutex), -1, me)) { sched_yield(); }

    if (shdata->idleCpus==0) { overloading=1; }

//  fprintf(stderr,"DLB DEBUG: (%d:%d) mutex: %d\n", node, me, shdata->mutex);
    //print_map();

    /*  if (shdata->idleCpus<cpus){ //There are not enough idle cpus
            fprintf(stderr,"DLB DEBUG: (%d:%d) Not cpus idle \n", node, me);
            i=0;
            while((shdata->num_cpus_proc[i]<=defaultCPUS) && i<procs) i++;

            if (i==procs) fprintf(stderr,"DLB ERROR: (%d:%d) Not enough cpus in other processes \n", node, me);
            else{


            }
        }else{  */      //I get only my cpus, if there are enough
    if (shdata->idleCpus<cpus) { cpus = shdata->idleCpus; } //if there are not enough cpus I only get the ones idle

    shdata->idleCpus-=cpus;

    memcpy(&(shdata->cpus_map[me][current_cpus]), &(shdata->cpus_map[procs][shdata->idleCpus]), sizeof(int)*cpus);

    for (i=0; i<cpus; i++) { shdata->cpus_map[procs][shdata->idleCpus+i]=-(me+101); }

    i=0;
    while((cpus+current_cpus<defaultCPUS) && i<procs) { //There are not enough idle cpus

//          fprintf(stderr,"DLB DEBUG: (%d:%d) Not enough cpus idle (%d) \n", node, me, cpus);

        while((shdata->num_cpus_proc[i] <= defaultCPUS) && i<procs) {
//fprintf(stderr,"DLB DEBUG: (%d:%d) proc %d have %d cpus \n", node, me, i, shdata->num_cpus_proc[i]);
            i++;
        }
        if(i==procs) { break; }

        //How many cpus can I get from this process?
        if(shdata->num_cpus_proc[i]-defaultCPUS< defaultCPUS-(cpus+current_cpus)) { tmp_cpus=shdata->num_cpus_proc[i]-defaultCPUS; }
        else { tmp_cpus=defaultCPUS-(cpus+current_cpus); }

//fprintf(stderr,"DLB DEBUG: (%d:%d) if %d - %d (%d) < %d + %d - %d (%d) tmp_cpus=%d else %d \n", node, me, shdata->num_cpus_proc[i], defaultCPUS, shdata->num_cpus_proc[i]-defaultCPUS ,cpus, current_cpus, defaultCPUS, (cpus+current_cpus)-defaultCPUS ,shdata->num_cpus_proc[i]-defaultCPUS, (cpus+current_cpus)-defaultCPUS);

//          fprintf(stderr, "DLB DEBUG (%d:%d) I'm getting %d cpus from %d\n", node, me, tmp_cpus, i);

        memcpy(&(shdata->cpus_map[me][current_cpus+cpus]), &(shdata->cpus_map[i][shdata->num_cpus_proc[i]-tmp_cpus]), sizeof(int)*tmp_cpus);

        shdata->claimed_cpus_proc[i]+=tmp_cpus;
        shdata->num_cpus_proc[i]-=tmp_cpus;
        cpus+=tmp_cpus;

    }

    if (i==procs) { fprintf(stderr,"DLB ERROR: (%d:%d) Not enough cpus in other processes \n", node, me); }

    memcpy(new_cpus, &(shdata->cpus_map[me][current_cpus]), sizeof(int)*cpus);
    shdata->num_cpus_proc[me]=current_cpus+cpus;

    if (current_cpus==0) { //I must set the master affinity
        cpu_set_t cpu_set;
        CPU_ZERO(&cpu_set);//prepare cpu_set
        CPU_SET(shdata->cpus_map[me][0], &cpu_set);
        if(sched_setaffinity(syscall(SYS_gettid), sizeof(cpu_set), &cpu_set)<0) { perror("ERROR: sched_setaffinity of master thread"); }
//add_event(1005, shdata->cpus_map[me][0]+1);

        if (overloading) { sched_yield(); }

    }

//  }
    //print_map();
//  fprintf(stderr,"DLB DEBUG: (%d:%d) mutex: %d\n", node, me, shdata->mutex);
    while(!__sync_bool_compare_and_swap(&(shdata->mutex), me, -1));
    //add_event(IDLE_CPUS_EVENT, shdata->idleCpus);

#ifdef debugMap
    fprintf(stderr,"DLB DEBUG: (%d:%d) Acquiring ", node, me);
    for (i=0; i<cpus; i++) {
        fprintf(stderr,"%d ", new_cpus[i]);
    }
    fprintf(stderr,"\n");
#endif

    return cpus+current_cpus;
}

/*
Returns de number of cpus
that are assigned
*/
int checkIdleCpus_Map(int myCpus, int max_cpus, int* new_cpus) {
    verbose(VB_SHMEM, "Checking idle CPUS... %d", shdata);
    int cpus=myCpus;
    int i;
    //if more CPUS than the availables are used release some
    if (shdata->claimed_cpus_proc[me]>0) {
        while(!__sync_bool_compare_and_swap(&(shdata->mutex), -1, me)) { sched_yield(); }
//fprintf(stderr,"DLB DEBUG: (%d:%d) I'm using %d, too many cpus... %d\n", node, me, shdata->num_cpus_proc[me], shdata->claimed_cpus_proc[me]);
        memcpy(new_cpus, &(shdata->cpus_map[me][shdata->num_cpus_proc[me]]), sizeof(int)*shdata->claimed_cpus_proc[me]);
        for (i=0; i<shdata->claimed_cpus_proc[me]; i++) {
            shdata->cpus_map[me][shdata->num_cpus_proc[me]+i]=-(me+201);
        }
        cpus=-shdata->num_cpus_proc[me];
        shdata->claimed_cpus_proc[me]=0;
        while(!__sync_bool_compare_and_swap(&(shdata->mutex), me, -1));

        //if there are idle CPUS use them
    } else if( shdata->idleCpus > 0 && max_cpus >0) {
        while(!__sync_bool_compare_and_swap(&(shdata->mutex), -1, me)) { sched_yield(); }
//fprintf(stderr,"DLB DEBUG: (%d:%d) Checking idle CPUS... %d\n", node, me, shdata->idleCpus);
        //print_map();

        cpus=shdata->idleCpus;
        if(cpus>defaultCPUS) { cpus=defaultCPUS; }
        if(cpus>max_cpus) { cpus=max_cpus; }

        memcpy(&(shdata->cpus_map[me][shdata->num_cpus_proc[me]]), &(shdata->cpus_map[procs][shdata->idleCpus-cpus]), sizeof(int)*cpus);
        memcpy(new_cpus, &(shdata->cpus_map[procs][shdata->idleCpus-cpus]), sizeof(int)*cpus);

        for (i=0; i<cpus; i++) { shdata->cpus_map[procs][shdata->idleCpus-i]=-(me+301); }

        shdata->num_cpus_proc[me]+=cpus;
        shdata->idleCpus-=cpus;

        cpus=shdata->num_cpus_proc[me];
        //print_map();
        while(!__sync_bool_compare_and_swap(&(shdata->mutex), me, -1));

    }
    //add_event(IDLE_CPUS_EVENT, shdata->idleCpus;

//  fprintf(stderr,"DLB DEBUG: (%d:%d) mutex: %d\n", node, me, shdata->mutex);
    return cpus;
}
