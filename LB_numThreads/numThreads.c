#include <numThreads.h>
#include <LB_MPI/tracing.h>
#include <LB_arch/arch.h>
#include <stdio.h>

//#include <mpitrace_user_events.h>


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
