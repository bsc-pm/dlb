#include <common.h>

int my_round(double x)
{
	int val = x;
	//return (int)((x+x+1.0)/2);
	if ((x - val)>0.5) val++;
	return val;
}

void atomic_add (int *x, int y){
	#pragma omp atomic
	*x += y;
}

void atomic_set (int *x, int y){
	*x = y;
}
