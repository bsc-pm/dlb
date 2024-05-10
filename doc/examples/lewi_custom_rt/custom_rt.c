
#include <dlb.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

void Extrae_event(unsigned type, long long value) __attribute__((weak));

enum { INITIAL_NUMTHREADS = 2 };
static int numthreads = INITIAL_NUMTHREADS;
static pid_t pid;

/* This is the callback that DLB will call every time there is a change
 * in the number of threads */
void callback_set_num_threads(int new_num_threads, void *arg) {
    printf("[CUSTOM RT %d]: Changing number of threads from %d to %d\n",
            pid, numthreads, new_num_threads);
    numthreads = new_num_threads;

    /* Optionally, if Extrae is preloaded we can emit the number of threads as
     * a custom event */
    if (Extrae_event) Extrae_event(111, numthreads);
}

__attribute__((constructor))
void init_runtime(void) {
    /* Save PID to identify print messages */
    pid = getpid();
    printf("[CUSTOM RT %d]: Initializing custom runtime with %d threads\n", pid, numthreads);

    /* Initialize DLB:
     *  - [ --lewi and NULL process mask ]: Configured for classic LeWI, with
     *      the initial number of threads and NO CPU affinity support. In this
     *      mode, DLB only sets the number of threads of each process, it will
     *      never set the CPU affinity of the threads.
     *
     *  - [ --lewi-mpi-calls=barrier ]: The subset of MPI calls that LeWI uses
     *      to determine when to lend or reclaim its resources can be
     *      configured, in this case it is only for MPI barrier.
     *
     *  - [ --mode=async ]: Configured for asynchronous mode. The runtime does
     *      not need to poll, it will receive callbacks asynchronously when the
     *      number of resources may be changed.
     */
    int error = DLB_Init(numthreads, /* no affinity */ NULL,
            "--mode=async --lewi --lewi-mpi-calls=barrier");
    if (error == DLB_SUCCESS) {
        printf("[CUSTOM RT %d]: DLB initialized correctly\n", pid);
    } else {
        printf("[CUSTOM RT %d]: DLB failed to initialize: %s\n", pid, DLB_Strerror(error));
        return;
    }

    /* Register DLB callback:
     *  - In this mode, DLB only changes the number of threads, not the CPU
     *      process affinity of each process. Thus, only
     *      dlb_callback_set_num_threads needs to be set.
     */
    error = DLB_CallbackSet(
            dlb_callback_set_num_threads,
            (dlb_callback_t) callback_set_num_threads,
            NULL);
    if (error == DLB_SUCCESS) {
        printf("[CUSTOM RT %d]: DLB callback registered successfully\n", pid);
    } else {
        printf("[CUSTOM RT %d]: DLB callback registration failed: %s\n", pid, DLB_Strerror(error));
        return;
    }

    /* Request the maximum number of CPUs for this process:
     *  - The runtime initially requests the maximum number of CPUs. Therefore,
     *      as soon as there are available CPUs, DLB will trigger the
     *      appropriate callback to increase the number of threads.
     *      The application may still lend and reclaim before and after MPI
     *      calls.
     *  - If, at some point, the application or runtime would want to reset
     *      this option, they would need to call:
     *          DLB_AcquireCpus(DLB_DELETE_REQUESTS);
     */
    error = DLB_AcquireCpus(DLB_MAX_CPUS);
    if (error == DLB_NOTED || error == DLB_SUCCESS) {
        printf("[CUSTOM RT %d]: DLB initial CPU request completed successfully\n", pid);
    } else {
        printf("[CUSTOM RT %d]: DLB initial CPU request failed: %s\n", pid, DLB_Strerror(error));
        return;
    }
}

__attribute__((destructor))
void finalize_runtime(void) {
    printf("[CUSTOM RT %d]: Finalizing custom runtime\n", pid);

    /* Finalize DLB */
    int error = DLB_Finalize();
    if (error == DLB_SUCCESS) {
        printf("[CUSTOM RT %d]: DLB finalized correctly\n", pid);
    } else {
        printf("[CUSTOM RT %d]: DLB failed to finalize: %s\n", pid, DLB_Strerror(error));
    }
}
