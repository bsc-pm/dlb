#ifndef NUMTHREADS_H
#define NUMTHREADS_H

#define _GNU_SOURCE
#include <sched.h>

int mpi_x_node;

void omp_set_num_threads (int numThreads) __attribute__ ((weak));
void css_set_num_threads (int numThreads) __attribute__ ((weak));
int* css_set_num_threads_cpus(int action, int num_cpus, int* cpus) __attribute__ ((weak));

void update_threads(int threads);
int* update_cpus(int action, int num_cpus, int* cpus);

void bind_master(int meId, int nodeId);
void DLB_bind_thread(int tid, int procsNode);
void bind_threads(int num_procs, int meId, int nodeId);

void get_mask( cpu_set_t *cpu_set );
void set_mask( cpu_set_t *cpu_set );
void add_mask( cpu_set_t *cpu_set );

#endif //NUMTHREADS_H
