#ifndef COMM_LEND_LIGHT_H
#define COMM_LEND_LIGHT_H

void ConfigShMem(int num_procs, int meId, int nodeId, int defCPUS, int is_greedy);

int releaseCpus(int cpus);

int acquireCpus();

int checkIdleCpus(int myCpus);

#endif //COMM_LEND_LIGHT