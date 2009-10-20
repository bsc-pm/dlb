#include <LB_arch/MyTime.h>
#include <stdio.h>


int iterNum;
struct timespec initAppl;
struct timespec initIter;
struct timespec initMPI;

struct timespec cpuTime;
struct timespec MPITime;

void JustProf_Init(void){
	//Read Environment vars
	if (clock_gettime(CLOCK_REALTIME, &initAppl)<0){
		fprintf(stderr, "clock_gettime failed\n");
	}
	reset(&cpuTime);
	reset(&MPITime);
	iterNum=0;
}

void JustProf_Finish(void){
	struct timespec aux;
	double secs;

	if (clock_gettime(CLOCK_REALTIME, &aux)<0){
		fprintf(stderr, "clock_gettime failed\n");
	}
	
	diff_time(initAppl, aux, &aux);

	secs=aux.tv_sec;
	secs+= (aux.tv_nsec) / 1e9;

	fprintf(stdout, "Application -> %.4f secs\n", secs);
}

void JustProf_InitIteration(void){
	if (clock_gettime(CLOCK_REALTIME, &initIter)<0){
		fprintf(stderr, "clock_gettime failed\n");
	}
	iterNum++;
	fprintf(stdout, "Iteration %d detected\n", iterNum);
}

void JustProf_FinishIteration(void){
	struct timespec aux;
	double secs;

	if (clock_gettime(CLOCK_REALTIME, &aux)<0){
		fprintf(stderr, "clock_gettime failed\n");
	}
	
	diff_time(initIter, aux, &aux);

	secs=aux.tv_sec;
	secs+= (aux.tv_nsec) / 1e9;
	fprintf(stdout, "Iteration %d -> %.4f secs\n", iterNum, secs);
	
}

void JustProf_IntoCommunication(void){}

void JustProf_OutOfCommunication(void){}

void JustProf_IntoBlockingCall(void){}

void JustProf_OutOfBlockingCall(void){}