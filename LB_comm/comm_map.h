#ifndef COMM_LEND_LIGHT_H
#define COMM_LEND_LIGHT_H

void ConfigShMem_Map(int num_procs, int meId, int nodeId, int defCPUS, int *my_cpus);

int releaseCpus_Map(int cpus, int*released_cpus);

int acquireCpus_Map(int current_cpus, int* new_cpus);

int checkIdleCpus_Map(int myCpus, int max_cpus, int* new_cpus);

void finalize_comm_Map();

#endif //COMM_LEND_LIGHT

