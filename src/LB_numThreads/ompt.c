
/*************************** Copied from LLVM-openmp's ompt.h *******************************/

/* Callbacks */
typedef enum ompt_callbacks_e{
    ompt_callback_thread_begin = 1, ompt_callback_thread_end = 2, ompt_callback_parallel_begin = 3, ompt_callback_parallel_end = 4, ompt_callback_task_create = 5, ompt_callback_task_schedule = 6, ompt_callback_implicit_task = 7, ompt_callback_control_tool = 11, ompt_callback_idle = 13, ompt_callback_sync_region_wait = 14, ompt_callback_mutex_released = 15, ompt_callback_task_dependences = 16, ompt_callback_task_dependence = 17, ompt_callback_work = 18, ompt_callback_master = 19, ompt_callback_sync_region = 21, ompt_callback_lock_init = 22, ompt_callback_lock_destroy = 23, ompt_callback_mutex_acquire = 24, ompt_callback_mutex_acquired = 25, ompt_callback_nest_lock = 26, ompt_callback_flush = 27, ompt_callback_cancel = 28,
} ompt_callbacks_t;
typedef void (*ompt_callback_t)(void);
typedef int (*ompt_set_callback_t) ( ompt_callbacks_t event, ompt_callback_t callback );


/* Start Tool */
typedef void (*ompt_interface_fn_t)(void);
typedef ompt_interface_fn_t (*ompt_function_lookup_t)(const char*);

struct ompt_fns_t;

typedef int (*ompt_initialize_t)(ompt_function_lookup_t ompt_fn_lookup, struct ompt_fns_t *fns);
typedef void (*ompt_finalize_t)(struct ompt_fns_t *fns);

typedef struct ompt_fns_t {
    ompt_initialize_t initialize;
    ompt_finalize_t finalize;
} ompt_fns_t;

/* Parallel begin/end callbacks */
#include <stdint.h>
typedef uint64_t ompt_id_t;
typedef union ompt_data_u {
  ompt_id_t value;
  void *ptr;
} ompt_data_t;
typedef ompt_data_t ompt_task_data_t;

typedef struct ompt_frame_s {
    void *exit_runtime_frame;
    void *reenter_runtime_frame;
} ompt_frame_t;

typedef enum {
    ompt_invoker_program = 1,
    ompt_invoker_runtime = 2
} ompt_invoker_t;

typedef void (*ompt_callback_parallel_begin_t) (
    ompt_data_t *parent_task_data,
    const ompt_frame_t *parent_frame,
    ompt_data_t *parallel_data,
    unsigned int requested_team_size,
    ompt_invoker_t invoker,
    const void *codeptr_ra
);

typedef void (*ompt_callback_parallel_end_t) (
    ompt_data_t *parallel_data,
    ompt_task_data_t *task_data,
    ompt_invoker_t invoker,
    const void *codeptr_ra
);

/* Implicit task callback */
typedef enum ompt_scope_endpoint_e {
    ompt_scope_begin = 1,
    ompt_scope_end = 2
} ompt_scope_endpoint_t;

typedef void (*ompt_callback_implicit_task_t) (
    ompt_scope_endpoint_t endpoint,
    ompt_data_t *parallel_data,
    ompt_data_t *task_data,
    unsigned int team_size,
    unsigned int thread_num
);

/* Thread begin/end callbacks */
typedef enum {
    ompt_thread_initial = 1,
    ompt_thread_worker = 2,
    ompt_thread_other = 3
} ompt_thread_type_t;

typedef void (*ompt_callback_thread_begin_t) (
    ompt_thread_type_t thread_type,
    ompt_data_t *thread_data
);

typedef void (*ompt_callback_thread_end_t) (
    ompt_data_t *thread_data
);

/********************************************************************************************/

#include "apis/DLB_interface.h"
#include "LB_comm/shmem_procinfo.h"
#include "LB_comm/shmem_cpuinfo.h"
#include "support/debug.h"
#include "support/mask_utils.h"
#include "support/tracing.h"
#include "apis/dlb_errors.h"

#include <inttypes.h>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>

int omp_get_thread_num(void) __attribute__((weak));
void omp_set_num_threads(int nthreads) __attribute__((weak));
int omp_get_level(void) __attribute__((weak));


/******************* OMP Thread Manager ********************/
static cpu_set_t active_mask;
static bool lewi = false;

static void cb_enable_cpu(int cpuid, void *arg) {
    CPU_SET(cpuid, &active_mask);
}

static void omp_thread_manager_acquire(void) {
    if (lewi) {
        DLB_Borrow();
        int nthreads = CPU_COUNT(&active_mask);
        omp_set_num_threads(nthreads);
        verbose(VB_OMPT, "Acquire - Setting new mask to %s", mu_to_str(&active_mask));
    }
}

static void omp_thread_manager_release(void) {
    if (lewi) {
        omp_set_num_threads(1);
        CPU_ZERO(&active_mask);
        CPU_SET(sched_getcpu(), &active_mask);
        verbose(VB_OMPT, "Release - Setting new mask to %s", mu_to_str(&active_mask));
        DLB_Lend();
    }
}

static void omp_thread_manager_init(void) {
    options_t options;
    options_init(&options, NULL);
    lewi = options.lewi && options.debug_opts & DBG_LEWI_OMPT;
    if (lewi) {
        cpu_set_t process_mask;
        sched_getaffinity(0, sizeof(cpu_set_t), &process_mask);
        DLB_Init(0, &process_mask, NULL);
        DLB_CallbackSet(dlb_callback_enable_cpu, (dlb_callback_t)cb_enable_cpu, NULL);
        omp_thread_manager_release();
    }
}

static void omp_thread_manager_finalize(void) {
    if (lewi) {
        DLB_Finalize();
    }
}
/***********************************************************/

static void cb_parallel_begin(
        ompt_data_t *parent_task_data,
        const ompt_frame_t *parent_frame,
        ompt_data_t *parallel_data,
        unsigned int requested_team_size,
        ompt_invoker_t invoker,
        const void *codeptr_ra) {
    if (omp_get_level() == 0) {
        omp_thread_manager_acquire();
    }
    /* warning("[%d] Encountered Parallel Construct, requested size: %u, invoker: %s", */
    /*         omp_get_thread_num(), */
    /*         requested_team_size, invoker == ompt_invoker_program ? "program" : "runtime"); */
    /* if (requested_team_size == 4) { */
    /*     warning("Modifying team size 4 -> 3"); */
    /*     omp_set_num_threads(3); */
    /* } */
}

static void cb_parallel_end(
        ompt_data_t *parallel_data,
        ompt_task_data_t *task_data,
        ompt_invoker_t invoker,
        const void *codeptr_ra) {
    if (omp_get_level() == 0) {
        omp_thread_manager_release();
    }
    /* warning("[%d] End of Parallel Construct, invoker: %s", omp_get_thread_num(), */
    /*         invoker == ompt_invoker_program ? "program" : "runtime"); */
}

static void cb_implicit_task(
        ompt_scope_endpoint_t endpoint,
        ompt_data_t *parallel_data,
        ompt_data_t *task_data,
        unsigned int team_size,
        unsigned int thread_num) {
    if (endpoint == ompt_scope_begin) {
        if (shmem_cpuinfo__is_dirty()) {
            int cpuid = shmem_cpuinfo__get_thread_binding(getpid(), thread_num);
            if (cpuid >= 0) {
                cpu_set_t thread_mask;
                CPU_ZERO(&thread_mask);
                CPU_SET(cpuid, &thread_mask);
                pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &thread_mask);
                add_event(REBIND_EVENT, cpuid);
            }
        }
    }
}

static void cb_thread_begin(
        ompt_thread_type_t thread_type,
        ompt_data_t *thread_data) {
    /* warning("Starting thread of type: %s", thread_type == ompt_thread_initial ? "initial" : */
    /*         thread_type == ompt_thread_worker ? "worker" : "other" ); */
}

static void cb_thread_end(
        ompt_data_t *thread_data) {
    /* warning("Ending thread"); */
}

static int ompt_initialize(ompt_function_lookup_t ompt_fn_lookup, ompt_fns_t *fns) {
    ompt_set_callback_t set_callback_fn = (ompt_set_callback_t)ompt_fn_lookup("ompt_set_callback");
    if (set_callback_fn) {
        set_callback_fn(ompt_callback_parallel_begin, (ompt_callback_t)cb_parallel_begin);
        set_callback_fn(ompt_callback_parallel_end,   (ompt_callback_t)cb_parallel_end);
        set_callback_fn(ompt_callback_implicit_task,  (ompt_callback_t)cb_implicit_task);
        set_callback_fn(ompt_callback_thread_begin,   (ompt_callback_t)cb_thread_begin);
        set_callback_fn(ompt_callback_thread_end,     (ompt_callback_t)cb_thread_end);

        omp_thread_manager_init();
    }
    return 0;
}

static void ompt_finalize(ompt_fns_t *fns) {
    omp_thread_manager_finalize();
}

static ompt_fns_t ompt_fns = { ompt_initialize, ompt_finalize };

#pragma GCC visibility push(default)
ompt_fns_t* ompt_start_tool(unsigned int omp_version, const char *runtime_version) {
    return &ompt_fns;
}
#pragma GCC visibility pop
