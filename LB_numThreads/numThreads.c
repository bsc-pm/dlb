#define _GNU_SOURCE

#include <sched.h>
#include <pthread.h>
#include <numThreads.h>
#include <LB_MPI/tracing.h>
#include <LB_arch/arch.h>
#include <stdio.h>


//#include <mpitrace_user_events.h>

void nanos_get_mask ( cpu_set_t *cpu_set ) __attribute__ ( ( weak ) );
void nanos_set_mask ( cpu_set_t *cpu_set ) __attribute__ ( ( weak ) );
void nanos_add_mask ( cpu_set_t *cpu_set ) __attribute__ ( ( weak ) );

int meId;
int nodeId;

void update_threads(int threads){
	if (threads>CPUS_NODE){
		fprintf(stderr, "WARNING trying to use more CPUS (%d) than the available (%d)\n", threads, CPUS_NODE);
		threads=CPUS_NODE;
	}

	add_event(THREADS_USED_EVENT, threads);
	if (threads==0){
		if(omp_set_num_threads)omp_set_num_threads(1);
		if(css_set_num_threads)css_set_num_threads(1);
	}else{
		if(omp_set_num_threads)omp_set_num_threads(threads);
		if(css_set_num_threads)css_set_num_threads(threads);
	}	
}

int* update_cpus(int action, int num_cpus, int* cpus){
	if (css_set_num_threads_cpus) return css_set_num_threads_cpus(action, num_cpus, cpus);
}

void bind_master(int me, int node){
	meId=me;
	nodeId=node;
	cpu_set_t cpu_set;
        CPU_ZERO(&cpu_set); 
	//cada thread principal a su CPU
	CPU_SET(meId, &cpu_set);
	if(sched_setaffinity(0, sizeof(cpu_set), &cpu_set)<0)perror("DLB ERROR: sched_setaffinity"); 
#ifdef debugBinding	
	fprintf(stderr, "DLB DEBUG: (%d:%d) Master Thread pinned to cpu %d\n", nodeId, meId, meId);
#endif
}
void bind_threads(int num_procs, int meId, int nodeId){
	#pragma omp parallel num_threads(CPUS_NODE)
	{
		DLB_bind_thread(omp_get_thread_num(), num_procs);
	}
}
void DLB_bind_thread(int tid, int procsNode){
		int i;
		cpu_set_t set;
		CPU_ZERO(&set);
                if (procsNode == 0) procsNode = mpi_x_node;
		int default_threads=CPUS_NODE/procsNode;

		//I am one of the default slave threads
		if(tid<(default_threads)){
			CPU_SET(tid+(meId*procsNode)%CPUS_NODE, &set);
#ifdef debugBinding	
			fprintf(stderr, "DLB DEBUG: (%d:%d) Thread %d pinned to cpu %d\n", nodeId, meId, tid, tid+(meId*procsNode)%CPUS_NODE);
#endif
		}else{
		//I am one of the auxiliar slave threads
			for (i=0; i<(CPUS_NODE-(default_threads)); i++){
				CPU_SET((i+((meId+1)*default_threads))%CPUS_NODE, &set);
#ifdef debugBinding	
				fprintf(stderr, "DLB DEBUG: (%d:%d) Thread %d pinned to cpu %d\n", nodeId, meId, tid, (i+((meId+1)*default_threads))%CPUS_NODE);
#endif
			}
		}
		if(sched_setaffinity(0, sizeof(set), &set)<0)perror("DLB ERROR: sched_setaffinity");
	
	if(pthread_setschedprio(pthread_self(), 1)<0)perror("DLB ERROR: pthread_setschedprio");
}


void get_mask( cpu_set_t *cpu_set )
{
   if ( nanos_get_mask ) nanos_get_mask ( cpu_set );
}

void set_mask( cpu_set_t *cpu_set )
{
   if ( nanos_set_mask ) nanos_set_mask ( cpu_set );
}

void add_mask( cpu_set_t *cpu_set )
{
   if ( nanos_add_mask ) nanos_add_mask ( cpu_set );
}

