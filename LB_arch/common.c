#include <common.h>

int round(double x)
{
	int val = x;
	//return (int)((x+x+1.0)/2);
	if ((x - val)>0.5) val++;
	return val;
}