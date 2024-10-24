
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <dlb.h>
#include <sched.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

static cpu_set_t available_cpu_set;

/* Print available_cpu_set */
static void print_info(void) {
    char buffer[CPU_SETSIZE*4];
    char *b = buffer;
    *(b++) = '[';
    bool entry_made = false;
    for (int cpuid = 0; cpuid < CPU_SETSIZE; ++cpuid) {
        if (CPU_ISSET(cpuid, &available_cpu_set)) {

            /* Find interval distance */
            int distance = 0;
            for (int next = cpuid + 1; next < CPU_SETSIZE; ++next) {
                if (CPU_ISSET(next, &available_cpu_set)) ++distance;
                else break;
            }

            /* Add ',' separator for subsequent entries */
            if (entry_made) {
                *(b++) = ',';
            } else {
                entry_made = true;
            }

            /* Write element, pair or range */
            if (distance == 0) {
                b += sprintf(b, "%d", cpuid);
            } else if (distance == 1) {
                b += sprintf(b, "%d,%d", cpuid, cpuid+1);
                ++cpuid;
            } else {
                b += sprintf(b, "%d-%d", cpuid, cpuid+distance);
                cpuid += distance;
            }
        }
    }
    *(b++) = ']';
    *b = '\0';

    printf("Affinity mask is now: %s\n", buffer);
}

/* Callbacks */

static void callback_enable_cpu(int cpuid, void *arg) {
    CPU_SET(cpuid, &available_cpu_set);
    print_info();
}

static void callback_enable_cpu_set(const cpu_set_t *cpuset, void *arg) {
    CPU_OR(&available_cpu_set, &available_cpu_set, cpuset);
    print_info();
}

static void callback_disable_cpu(int cpuid, void *arg) {
    CPU_CLR(cpuid, &available_cpu_set);
    print_info();
}

static void callback_disable_cpu_set(const cpu_set_t *cpuset, void *arg) {
    cpu_set_t xor;
    CPU_XOR(&xor, &available_cpu_set, cpuset);
    CPU_AND(&available_cpu_set, &available_cpu_set, &xor);
    print_info();
}

static void init_dlb(void) {
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
    CPU_ZERO(&available_cpu_set);
    int error = DLB_Init(0, &available_cpu_set, "--mode=async --lewi");
    if (error == DLB_SUCCESS) {
        printf("DLB initialized correctly\n");
    } else {
        printf("DLB failed to initialize: %s\n", DLB_Strerror(error));
        return;
    }

    /* Register DLB callbacks: */

    /* enable_cpu: */
    error = DLB_CallbackSet(
            dlb_callback_enable_cpu,
            (dlb_callback_t) callback_enable_cpu,
            NULL);
    if (error == DLB_SUCCESS) {
        printf("DLB callback enable_cpu registered successfully\n");
    } else {
        printf("DLB callback enable_cpu registration failed: %s\n", DLB_Strerror(error));
        return;
    }

    /* enable_cpu_set: */
    error = DLB_CallbackSet(
            dlb_callback_enable_cpu_set,
            (dlb_callback_t) callback_enable_cpu_set,
            NULL);
    if (error == DLB_SUCCESS) {
        printf("DLB callback enable_cpu_set registered successfully\n");
    } else {
        printf("DLB callback enable_cpu_set registration failed: %s\n", DLB_Strerror(error));
        return;
    }

    /* disable_cpu: */
    error = DLB_CallbackSet(
            dlb_callback_disable_cpu,
            (dlb_callback_t) callback_disable_cpu,
            NULL);
    if (error == DLB_SUCCESS) {
        printf("DLB callback disable_cpu registered successfully\n");
    } else {
        printf("DLB callback disable_cpu registration failed: %s\n", DLB_Strerror(error));
        return;
    }

    /* disable_cpu_set: */
    error = DLB_CallbackSet(
            dlb_callback_disable_cpu_set,
            (dlb_callback_t) callback_disable_cpu_set,
            NULL);
    if (error == DLB_SUCCESS) {
        printf("DLB callback disable_cpu_set registered successfully\n");
    } else {
        printf("DLB callback disable_cpu_set registration failed: %s\n", DLB_Strerror(error));
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
        printf("DLB initial CPU request completed successfully\n");
    } else {
        printf("DLB initial CPU request failed: %s\n", DLB_Strerror(error));
        return;
    }
}

static void finalize_dlb(void) {
    int error = DLB_Finalize();
    if (error == DLB_SUCCESS) {
        printf("DLB finalized correctly\n");
    } else {
        printf("DLB failed to finalize: %s\n", DLB_Strerror(error));
    }
}

int main(int argc, char *argv[]) {
    init_dlb();

    printf("Asking for resources indefinitely. Type 'q<Enter>' to quit.\n");
    while (1) {
        char key = getchar();
        if (key == 'q') break;
    }

    finalize_dlb();
}
