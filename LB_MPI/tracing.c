#include <tracing.h>


void add_event(int type, int value){
	if(MPItrace_eventandcounters) MPItrace_eventandcounters(type,value);
}
