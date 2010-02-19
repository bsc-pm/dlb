#include <MyTime.h>

int diff_time(struct timespec init, struct timespec end, struct timespec* diff){
	if (init.tv_sec>end.tv_sec){
		return -1;
	}else{
		if ((init.tv_sec==end.tv_sec)&&(init.tv_nsec>end.tv_nsec)){
			return -1;
		}else{
			if (init.tv_nsec>end.tv_sec){
				diff->tv_sec = end.tv_sec - (init.tv_sec + 1);
				diff->tv_nsec = (end.tv_nsec + 1e9)-init.tv_nsec;
			}else{
				diff->tv_sec = end.tv_sec - init.tv_sec;
				diff->tv_nsec = end.tv_nsec - init.tv_nsec;
			}
		}
	}
	return 0;
}

int add_time(struct timespec t1, struct timespec t2, struct timespec* sum){
	sum->tv_nsec = t1.tv_nsec + t2.tv_nsec;

	if (sum->tv_nsec >= 1e9){
		sum->tv_sec = t1.tv_sec + t2.tv_sec + (sum->tv_nsec/1e9);
		sum->tv_nsec = (sum->tv_nsec % (long)1e9);
	}else{
		sum->tv_sec = t1.tv_sec + t2.tv_sec;
	}
}

void reset(struct timespec *t1){
	t1->tv_nsec=0;
	t1->tv_sec=0;
}

double to_secs(struct timespec t1){
	double secs;

	secs=t1.tv_sec;
	secs+= (t1.tv_nsec) / 1e9;
	
	return secs;
}

