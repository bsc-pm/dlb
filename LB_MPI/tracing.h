

void MPItrace_eventandcounters (int Type, int Value)__attribute__ ((weak));

void add_event(int type, int value);

/********* OMPITRACE EVENTS *********/
#define THREADS_USED 800000
#define PROCESS_EVENT 800001
#define MASTER_STATE 800002
#define PROC_THREADS 800010
#define RUNTIME 800020
/*************************************/
