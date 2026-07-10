#include <dlb_talp.h>
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
     * Pattern (2x2 threads):
     *
     * Inside the parallel region:
     *   threads get unequal work, so faster ones wait.
     *     thread 0: 0.5s, thread 1: 0.25s
     *
     *   In nested parallel:
     *     thread 0 descendants do some work
     *     thread 1 descendants do nothing
     */

    dlb_monitor_t *monitor_thread = DLB_MonitoringRegionRegister("Thread");

    /* --- Serial section: only master runs, workers idle (serialization) --- */
    busy_wait(0.15);

    /* --- Parallel region --- */
    #pragma omp parallel num_threads(2)
    {
        int tid = omp_get_thread_num();

        /* Load imbalance: each thread gets a different amount of work */
        double work = 0.5 - tid * 0.25;   /* 0.5, 0.25 */

        DLB_MonitoringRegionStart(monitor_thread);
        if (work > 0) busy_wait(work);
        DLB_MonitoringRegionStop(monitor_thread);

        /* threads finish at different times → imbalance at the implicit barrier */

        #pragma omp barrier

        #pragma omp parallel num_threads(2) shared(tid, work)
        {
            /* Threads 0.0 and 0.1 do some work in a thread local region */
            if (tid == 0) {
                DLB_MonitoringRegionStart(monitor_thread);
                if (work > 0) busy_wait(work);
                DLB_MonitoringRegionStop(monitor_thread);
            }
        }
    }

    /* --- Empty small parallel region to test num_cpus is correct --- */
    dlb_monitor_t *monitor_small = DLB_MonitoringRegionRegister("Small");
    DLB_MonitoringRegionStart(monitor_small);
    #pragma omp parallel num_threads(1)
    {
        busy_wait(0.10);
    }
    DLB_MonitoringRegionStop(monitor_small);
}

int main(int argc, char *argv[]) {

    int repetitions = argc > 1 ? atoi(argv[1]) : 1;

    for (int i = 0; i < repetitions; ++i) {
        run_test();
    }

    return 0;
}
