#include <numThreads.h>
//#include <mpitrace_user_events.h>

void OMPItrace_event(int Type, int Value)__attribute__ ((weak));

void update_threads(int threads){
//#ifdef OMPI_events	
	if(OMPItrace_event)OMPItrace_event (800000, threads);
//#endif
	if (threads==0){
		if(omp_set_num_threads)omp_set_num_threads(1);
		if(css_set_num_threads)css_set_num_threads(1);
	}else{
		if(omp_set_num_threads)omp_set_num_threads(threads);
		if(css_set_num_threads)css_set_num_threads(threads);
	}	
}
