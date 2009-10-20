#include <openMP_interface.h>
#include <mpitrace_user_events.h>

void update_threads(int threads){
#ifdef OMPI_events	
	OMPItrace_event (800000, threads);
#endif
	if (threads==0){
		omp_set_num_threads(1);
	}else{
		omp_set_num_threads(threads);
	}	
}