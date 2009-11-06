#ifndef NUMTHREADS_H
#define NUMTHREADS_H

void omp_set_num_threads (int numThreads) __attribute__ ((weak));
void css_set_num_threads (int numThreads) __attribute__ ((weak));


void update_threads(int threads);

#endif //NUMTHREADS_H
