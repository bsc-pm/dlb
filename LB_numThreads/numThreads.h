#ifndef NUMTHREADS_H
#define NUMTHREADS_H

void omp_set_num_threads (int numThreads) __attribute__ ((weak));
void css_set_num_threads (int numThreads) __attribute__ ((weak));
int* css_set_num_threads_cpus(int action, int num_cpus, int* cpus);

void update_threads(int threads);
int* update_cpus(int action, int num_cpus, int* cpus);

#endif //NUMTHREADS_H
