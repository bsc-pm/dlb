

void MPItrace_eventandcounters (int Type, int Value)__attribute__ ((weak));

void add_event(int type, int value);

/********* OMPITRACE EVENTS *********/
#define THREADS_USED_EVENT 800000
#define RUNTIME_EVENT 800020
#define IDLE_CPUS_EVENT 800030
#define BIND_2CPU_EVENT 800040
/*************************************/
