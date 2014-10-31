#include <Lend.h>
#include <LB_numThreads/numThreads.h>
//#include <mpitrace_user_events.h>

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

int me, node, procs;
int finished;
int threads2use, threadsUsed;
int idleCpus;

/******* Main Functions Lend Balancing Policy ********/

void Lend_Init(int meId, int num_procs, int nodeId) {
#ifdef debugConfig
    fprintf(stderr, "%d:%d - Lend Init\n", nodeId, meId);
#endif
    me = meId;
    node = nodeId;
    procs = num_procs;

    //Create auxiliar threads
    createThreads_Lend();
}

void Lend_Finish(void) {
    finished=1;
    comm_close();
}

void Lend_InitIteration() {}

void Lend_FinishIteration(double cpuSecs, double MPISecs) {}

void Lend_IntoCommunication(void) {
}

void Lend_OutOfCommunication(void) {
    Lend_updateresources();
}

void Lend_IntoBlockingCall(double cpuSecs, double MPISecs) {
    ProcMetrics info;
#ifdef debugLend
    fprintf(stderr, "%d:%d - LENDING %d cpus\n", node, me, threadsUsed);
#endif
    info.action = LEND_CPUS;
    info.cpus=threadsUsed;
    info.secsComp=cpuSecs;
    info.secsMPI=MPISecs;

    threads2use=0;
    Lend_updateresources();
    SendToMaster((char *)&info,sizeof(info));
    threads2use=0;
    Lend_updateresources();
}

void Lend_OutOfBlockingCall(void) {
    ProcMetrics info;
    info.action = ACQUIRE_CPUS;
    info.cpus=threadsUsed;

    threads2use=_default_nthreads;
    Lend_updateresources();
    SendToMaster((char *)&info,sizeof(info));
    threads2use=_default_nthreads;
    Lend_updateresources();
#ifdef debugLend
    fprintf(stderr, "%d:%d - ACQUIRING %d cpus\n", node, me, threadsUsed);
#endif
}

/******* Auxiliar Functions Lend Balancing Policy ********/

/* Creates auxiliar threads */
int createThreads_Lend() {
    pthread_t t;
    finished=0;

    threads2use=_default_nthreads;

#ifdef debugConfig
    fprintf(stderr, "%d:%d - Creating Threads\n", node, me);
#endif
    LoadCommConfig(procs, me, node);

    if (me==0) {
        if (pthread_create(&t,NULL,masterThread_Lend,NULL)>0) {
            perror("createThreads:Error in pthread_create master\n");
            exit(1);
        }
    }

    if (pthread_create(&t,NULL,slaveThread_Lend,NULL)>0) {
        perror("createThreads:Error in pthread_create slave\n");
        exit(1);
    }
}

/******* Master Thread Functions ********/
void* masterThread_Lend(void* arg) {

    int cpus[procs];
    int i, aux;
    //int info;
    ProcMetrics info;
    ProcMetrics old_info[procs];

#ifdef debugConfig
    fprintf(stderr,"%d:%d - Creating Master thread\n", node, me);
#endif

    StartMasterComm();
    idleCpus=0;


    //We start with equidistribution
    for (i=0; i<procs; i++) { cpus[i]=_default_nthreads; }

    applyNewDistribution(cpus, procs+1);

    while(!finished) {
        i = GetFromAnySlave((char*)&info, sizeof(info));

        if (info.action ==LEND_CPUS) {
            assign_lended_cpus(cpus, i, old_info);
        } else {
            retrieve_cpus(cpus, i, old_info);
        }

#ifdef debugDistribution
        aux=0;
        fprintf(stderr,"%d:%d - New Distribution: ", node, me);
        for (i=0; i<procs; i++) { fprintf(stderr,"[%d]", cpus[i]); aux+=cpus[i];}
        fprintf(stderr,"Total=%d\n", aux);
#endif
    }
    return NULL;
}

void applyNewDistribution(int * cpus, int except) {
    int i;
#ifdef debugDistribution
    fprintf(stderr,"%d:%d - New Distribution: ", node, me);
    for (i=0; i<procs; i++) { fprintf(stderr,"[%d]", cpus[i]); }
    fprintf(stderr,"\n");
#endif
    for (i=0; i<procs; i++) {
        if(i!=except) {
            SendToSlave(i,(char*) &cpus[i] , sizeof(int));
        }
    }
}

int assign_lended_cpus(int* cpus, int process, ProcMetrics info[]) {
    int i;
    int lended;
    double load;
    double max;
    int loadedProc;

    if(cpus[process]==0) {
        fprintf(stderr,"%d:%d - WARNING proces %d trying to Lend cpus when does not have any\n", node, me, process);
    } else {
        lended=cpus[process];



        while (lended>0) {
            max=-1;
            loadedProc=-1;

            i=(process+1)%procs;
            while(i!=process) {
                if(cpus[i]!=0) {
                    load=(double)info[i].secsComp/(double)cpus[i];
                } else {
                    load=-1;
                }
//fprintf(stderr,"%d:%d - Process %d has load %.4f (%.4f/%d)\n", node, me, i, load, info[i].secsComp, cpus[i]);
                if (load>max) {
                    max=load;
                    loadedProc=i;
                }
                i=(i+1)%procs;
            }

            if (loadedProc!=-1) {
                cpus[loadedProc]++;
                lended--;
                SendToSlave(loadedProc,(char*) &cpus[loadedProc] , sizeof(int));
#ifdef debugLend
                fprintf(stderr,"%d:%d - Process %d LENDED %d cpus to process %d\n", node, me, process, 1, loadedProc);
#endif
            } else {
                idleCpus+=lended;
                lended=0;
            }

        }
        cpus[process]=0;
    }
}

int retrieve_cpus(int* cpus, int process, ProcMetrics info[]) {
    if(cpus[process]!=0) {
        fprintf(stderr,"%d:%d - WARNING proces %d trying to Retrieve cpus when have already %d\n", node, me, process, cpus[process]);
    } else {
        int missing=_default_nthreads;
        int i;
        if(idleCpus>=(_default_nthreads)) {
            idleCpus-=missing;
            missing=0;
#ifdef debugLend
            fprintf(stderr,"%d:%d - Process %d ACQUIRED %d cpus that where idle\n", node, me, process, _default_nthreads);
#endif
        } else {

#ifdef debugLend
            fprintf(stderr,"%d:%d - Process %d ACQUIRED %d cpus that where idle\n", node, me, process, idleCpus);
#endif
            missing=missing-idleCpus;
            idleCpus=0;

            i= (process+1)%procs;
            while ((missing!=0) && (i!=process)) {

                if (cpus[i]>_default_nthreads) {
                    if (missing>(cpus[i]-_default_nthreads)) {
#ifdef debugLend
                        fprintf(stderr,"%d:%d - Process %d ACQUIRED %d cpus from %d\n", node, me, process, cpus[i]-_default_nthreads, i);
#endif
                        missing=missing-(cpus[i]-_default_nthreads);
                        cpus[i]=_default_nthreads;
                    } else {
#ifdef debugLend
                        fprintf(stderr,"%d:%d - Process %d ACQUIRED %d cpus from %d\n", node, me, process, missing, i);
#endif
                        cpus[i]=cpus[i]-missing;
                        missing=0;
                    }
                    SendToSlave(i,(char*) &cpus[i] , sizeof(int));
                }
                i= (i+1)%procs;
            }
        }
        cpus[process]=(_default_nthreads);
        //SendToSlave(process,(char*) &cpus[process] , sizeof(int));
        if(missing!=0) {
            fprintf(stderr,"%d:%d - WARNING proces %d tried to retrieve %d cpus and there are not enough available\n", node, me, process, _default_nthreads);
        }
    }
}
/* Pasa las cpus liberadas al que tiene mas tiempo de cpu */
int CalculateNewDistribution_Lend(int* cpus, int process, ProcMetrics info[]) {
    int i;
    int lended;
    int changed = -1;
    double load;
    double max;
    int loadedProc;

    if (info[process].action == LEND_CPUS) {
        if(cpus[process]==0) {
            fprintf(stderr,"%d:%d - WARNING proces %d trying to Lend cpus when does not have any\n", node, me, process);
        } else {
            lended=cpus[process];

            max=-1;
            loadedProc=-1;

            i=(process+1)%procs;
            while(i!=process) {
                if(cpus[i]!=0) {
                    load=(double)info[i].secsComp/(double)cpus[i];
                } else {
                    load=0;
                }
//fprintf(stderr,"%d:%d - Process %d has load %.4f (%.4f/%d)\n", node, me, i, load, info[i].secsComp, cpus[i]);
                if (load>max) {
                    max=load;
                    loadedProc=i;
                }
                i=(i+1)%procs;
            }

            if (loadedProc!=-1) {
                cpus[loadedProc]+=lended;
                changed=loadedProc;
#ifdef debugLend
                fprintf(stderr,"%d:%d - Process %d LENDED %d cpus to process %d\n", node, me, process, lended, loadedProc);
#endif
            } else {
                idleCpus+=lended;
#ifdef debugLend
                fprintf(stderr,"%d:%d - Process %d LENDED %d cpus to be idle\n", node, me, process, lended);
#endif
            }
            cpus[process]=0;


        }
    } else {
        if(cpus[process]!=0) {
            fprintf(stderr,"%d:%d - WARNING proces %d trying to Retrieve cpus when have already %d\n", node, me, i, cpus[process]);
        } else {
            lended=0;
            i= (process+1)%procs;

            if(idleCpus>=(_default_nthreads)) {
                lended=_default_nthreads;
                idleCpus-=lended;
#ifdef debugLend
                fprintf(stderr,"%d:%d - Process %d ACQUIRED %d cpus that where idle\n", node, me, process, lended);
#endif
            } else {
                lended=idleCpus;
                idleCpus=0;

                while ((lended<(_default_nthreads)) && (i!=process)) {

                    if (cpus[i]>_default_nthreads) {
#ifdef debugLend
                        fprintf(stderr,"%d:%d - Process %d ACQUIRED %d cpus from %d\n", node, me, process, cpus[i]-_default_nthreads, i);
#endif
                        lended=lended+(cpus[i]-_default_nthreads);
                        cpus[i]=_default_nthreads;
                        changed=i;
                    }
                    i= (i+1)%procs;
                }

            }
            //cpus[process]=_default_nthreads;
            cpus[process]=lended;
        }
    }
    return changed;
}


/******* Slave Thread Functions ********/
void* slaveThread_Lend(void* arg) {
    int cpus;

#ifdef debugConfig
    fprintf(stderr,"%d:%d - Creating Slave thread\n", node, me);
#endif
    StartSlaveComm();
    threadsUsed=_default_nthreads;

    while(!finished) {
        GetFromMaster((char*)&cpus,sizeof(int));
        threads2use=cpus;
    }
}

void Lend_updateresources() {
    if (threadsUsed!=threads2use) {
#ifdef debugDistribution
        fprintf(stderr,"%d:%d - Using %d cpus\n", node, me, threads2use);
#endif
        update_threads(threads2use);
        threadsUsed=threads2use;
    }
}

