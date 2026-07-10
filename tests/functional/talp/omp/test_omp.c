#include <omp.h>
#include <stdint.h>
#include <stdlib.h>

static void busy_wait(double seconds) {
    double start = omp_get_wtime();
    volatile uint64_t x = 0;
    while (omp_get_wtime() - start < seconds) x++;
}

void run_test(void) {
    /*
     * Pattern (4 threads):
     *
     * ompSerializationTime: threads 1-3 are idle during serial sections.
     *   ~1.0s of serial work before the parallel region → 3 threads * 1.0s = ~3.0s
     *
     * Inside the parallel region:
     *   ompLoadImbalanceTime: threads get unequal work, so faster ones wait.
     *     thread 0: 1.0s, thread 1: 0.75s, thread 2: 0.5s, thread 3: 0.25s
     *     imbalance lost: (1.0-1.0)+(1.0-0.75)+(1.0-0.5)+(1.0-0.25) = ~1.5s
     *
     *   ompSchedulingTime: a worksharing loop with dynamic schedule forces
     *     overhead and synchronization between threads. Small but measurable.
     */

    /* --- Serial section: only master runs, workers idle (serialization) --- */
    busy_wait(1.0);   /* 1.0s * (nthreads-1) attributed as ompSerializationTime */

    /* --- Parallel region --- */
    #pragma omp parallel num_threads(4)
    {
        int tid = omp_get_thread_num();

        /* Load imbalance: each thread gets a different amount of work */
        double work = 1.0 - tid * 0.25;   /* 1.0, 0.75, 0.5, 0.25 */
        if (work > 0) busy_wait(work);
        /* threads finish at different times → imbalance at the implicit barrier */

        #pragma omp barrier

        /* Dynamic scheduling loop: introduces scheduling overhead */
        #pragma omp for schedule(dynamic, 1)
        for (int i = 0; i < 100; i++) {
            busy_wait(0.005);  /* 100 * 5ms = 0.5s total work, split dynamically */
        }
        /* implicit barrier here contributes more imbalance/scheduling time */
    }
}

int main(int argc, char *argv[]) {

    int repetitions = argc > 1 ? atoi(argv[1]) : 1;

    for (int i = 0; i < repetitions; ++i) {
        run_test();
    }

    return 0;
}
