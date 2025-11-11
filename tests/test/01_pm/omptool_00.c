/*********************************************************************************/
/*  Copyright 2009-2024 Barcelona Supercomputing Center                          */
/*                                                                               */
/*  This file is part of the DLB library.                                        */
/*                                                                               */
/*  DLB is free software: you can redistribute it and/or modify                  */
/*  it under the terms of the GNU Lesser General Public License as published by  */
/*  the Free Software Foundation, either version 3 of the License, or            */
/*  (at your option) any later version.                                          */
/*                                                                               */
/*  DLB is distributed in the hope that it will be useful,                       */
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of               */
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                */
/*  GNU Lesser General Public License for more details.                          */
/*                                                                               */
/*  You should have received a copy of the GNU Lesser General Public License     */
/*  along with DLB.  If not, see <https://www.gnu.org/licenses/>.                */
/*********************************************************************************/

/*<testinfo>
    test_generator="gens/basic-generator"
</testinfo>*/

#include "unique_shmem.h"

#include "LB_numThreads/omp-tools.h"
#include "LB_numThreads/omptool.h"
#include "apis/dlb_errors.h"
#include "support/env.h"

#include <sched.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>


ompt_start_tool_result_t* ompt_start_tool(
        unsigned int omp_version, const char *runtime_version);

typedef struct event_checkpoints_t {
    bool init;
    bool finalize;
    bool into_mpi;
    bool outof_mpi;
    bool lend_from_api;
    bool thread_begin;
    bool thread_end;
    bool thread_role_shift;
    bool parallel_begin;
    bool parallel_end;
    bool into_parallel_function;
    bool into_parallel_implicit_barrier;
    bool task_create;
    bool task_complete;
    bool task_switch;
} event_checkpoints_t;

static event_checkpoints_t talp_event = {0};
static event_checkpoints_t omptm_event = {0};
static void reset_event_checkpoints(void) {
    talp_event = (const event_checkpoints_t) {0};
    omptm_event = (const event_checkpoints_t) {0};
}


/*** TALP event functions ***/
static void talp_event_init(pid_t pid, const options_t* options) {
    talp_event.init = true;
}

static void talp_event_finalize(void) {
    talp_event.finalize = true;
}

static void talp_event_into_mpi(void) {
    talp_event.into_mpi = true;
}

static void talp_event_outof_mpi(void) {
    talp_event.outof_mpi = true;
}

static void talp_event_lend_from_api(void) {
    talp_event.lend_from_api = true;
}

static void talp_event_thread_begin(ompt_thread_t thread) {
    talp_event.thread_begin = true;
}

static void talp_event_thread_end(void) {
    talp_event.thread_end = true;
}

static void talp_event_thread_role_shift(ompt_data_t* data, ompt_role_t prior_role,
        ompt_role_t next_role) {
    talp_event.thread_role_shift = true;
}

static void talp_event_parallel_begin(omptool_parallel_data_t* data) {
    talp_event.parallel_begin = true;
}

static void talp_event_parallel_end(omptool_parallel_data_t* data) {
    talp_event.parallel_end = true;
}

static void talp_event_into_parallel_function(omptool_parallel_data_t* data,
        unsigned int index) {
    talp_event.into_parallel_function = true;
}

static void talp_event_into_parallell_implicit_barrier(omptool_parallel_data_t* data) {
    talp_event.into_parallel_implicit_barrier = true;
}

static void talp_event_task_create(void) {
    talp_event.task_create = true;
}

static void talp_event_task_complete(void) {
    talp_event.task_complete = true;
}

static void talp_event_task_switch(void) {
    talp_event.task_switch = true;
}

/*** OMPTM event functions ***/
static void omptm_event_init(pid_t pid, const options_t* options) {
    omptm_event.init = true;
}

static void omptm_event_finalize(void) {
    omptm_event.finalize = true;
}

static void omptm_event_into_mpi(void) {
    omptm_event.into_mpi = true;
}

static void omptm_event_outof_mpi(void) {
    omptm_event.outof_mpi = true;
}

static void omptm_event_lend_from_api(void) {
    omptm_event.lend_from_api = true;
}

static void omptm_event_thread_begin(ompt_thread_t thread) {
    omptm_event.thread_begin = true;
}

static void omptm_event_thread_end(void) {
    omptm_event.thread_end = true;
}

static void omptm_event_thread_role_shift(ompt_data_t* data, ompt_role_t prior_role,
        ompt_role_t next_role) {
    omptm_event.thread_role_shift = true;
}

static void omptm_event_parallel_begin(omptool_parallel_data_t* data) {
    omptm_event.parallel_begin = true;
}

static void omptm_event_parallel_end(omptool_parallel_data_t* data) {
    omptm_event.parallel_end = true;
}

static void omptm_event_into_parallel_function(omptool_parallel_data_t* data,
        unsigned int index) {
    omptm_event.into_parallel_function = true;
}

static void omptm_event_into_parallell_implicit_barrier(omptool_parallel_data_t* data) {
    omptm_event.into_parallel_implicit_barrier = true;
}

static void omptm_event_task_create(void) {
    omptm_event.task_create = true;
}

static void omptm_event_task_complete(void) {
    omptm_event.task_complete = true;
}

static void omptm_event_task_switch(void) {
    omptm_event.task_switch = true;
}

/*** Fake OpenMP runtime ***/
static omptool_callback_funcs_t callbacks = {0};
static ompt_set_result_t set_callback(ompt_callbacks_t event, ompt_callback_t callback) {
    ompt_set_result_t result;
    switch(event) {
        case ompt_callback_thread_begin:
            callbacks.thread_begin = (ompt_callback_thread_begin_t)callback;
            result = ompt_set_always;
            break;
        case ompt_callback_thread_end:
            callbacks.thread_end = (ompt_callback_thread_end_t)callback;
            result = ompt_set_always;
            break;
        case ompt_callback_thread_role_shift:
            callbacks.thread_role_shift = (ompt_callback_thread_role_shift_t)callback;
            result = ompt_set_always;
            break;
        case ompt_callback_parallel_begin:
            callbacks.parallel_begin = (ompt_callback_parallel_begin_t)callback;
            result = ompt_set_always;
            break;
        case ompt_callback_parallel_end:
            callbacks.parallel_end = (ompt_callback_parallel_end_t)callback;
            result = ompt_set_always;
            break;
        case ompt_callback_task_create:
            callbacks.task_create = (ompt_callback_task_create_t)callback;
            result = ompt_set_always;
            break;
        case ompt_callback_task_schedule:
            callbacks.task_schedule = (ompt_callback_task_schedule_t)callback;
            result = ompt_set_always;
            break;
        case ompt_callback_implicit_task:
            callbacks.implicit_task = (ompt_callback_implicit_task_t)callback;
            result = ompt_set_always;
            break;
        case ompt_callback_sync_region:
            callbacks.sync_region = (ompt_callback_sync_region_t)callback;
            result = ompt_set_always;
            break;
        default:
            result = ompt_set_error;
            break;
    }
    return result;
}

static void* lookup_fn (const char *interface_function_name) {
    if (strcmp(interface_function_name, "ompt_set_callback") == 0) {
            return set_callback;
    }
    return NULL;
}

static void test_omptool_events(ompt_start_tool_result_t *tool_result,
        bool talp, bool omptm) {
    ompt_data_t tool_data = {};
    ompt_data_t thread_data = {};
    ompt_data_t parallel_data_l1 = {};
    ompt_data_t parallel_data_l2 = {};
    ompt_data_t task_data = {};
    ompt_data_t new_task_data = {};
    ompt_data_t encountering_task_data = {};
    ompt_frame_t encountering_task_frame = {.exit_frame.ptr = NULL};
    ompt_frame_t nested_encountering_task_frame = {.exit_frame.ptr = (void*)0x42};
    unsigned int requested_parallelism = 1;
    const void *codeptr_ra = (void*)0x42;

    // init
    reset_event_checkpoints();
    tool_result->initialize((ompt_function_lookup_t)lookup_fn, 0, &tool_data);
    assert( talp_event.init  == talp );
    assert( omptm_event.init == omptm );
    assert( callbacks.thread_begin != NULL );
    assert( callbacks.thread_end != NULL );
    assert( callbacks.thread_role_shift == NULL );  /* disabled for omp5 */
    assert( callbacks.parallel_begin != NULL );
    assert( callbacks.parallel_end != NULL );
    assert( callbacks.task_create != NULL );
    assert( callbacks.task_schedule != NULL );
    assert( callbacks.implicit_task != NULL );
    assert( callbacks.sync_region != NULL );

    // thread begin
    reset_event_checkpoints();
    callbacks.thread_begin(ompt_thread_initial, &thread_data);
    assert( talp_event.thread_begin  == talp );
    assert( omptm_event.thread_begin == omptm );

    // thread end
    reset_event_checkpoints();
    callbacks.thread_end(&thread_data);
    assert( talp_event.thread_end  == talp );
    assert( omptm_event.thread_end == omptm );

    // into mpi
    reset_event_checkpoints();
    omptool__into_blocking_call();
    assert( talp_event.into_mpi  == talp );
    assert( omptm_event.into_mpi == omptm );

    // out of mpi
    reset_event_checkpoints();
    omptool__outof_blocking_call();
    assert( talp_event.outof_mpi  == talp );
    assert( omptm_event.outof_mpi == omptm );

    // lend from api
    reset_event_checkpoints();
    omptool__lend_from_api();
    assert( talp_event.lend_from_api  == false );   /* not implemented */
    assert( omptm_event.lend_from_api == omptm );

    // parallel begin
    reset_event_checkpoints();
    callbacks.parallel_begin(&encountering_task_data, &encountering_task_frame,
            &parallel_data_l1, requested_parallelism, 0, codeptr_ra); /* not a parallel team */
    assert( talp_event.parallel_begin  == false );
    assert( omptm_event.parallel_begin == false );
    callbacks.parallel_begin(&encountering_task_data, &encountering_task_frame,
            &parallel_data_l1, requested_parallelism, ompt_parallel_team, codeptr_ra);
    assert( talp_event.parallel_begin  == talp );
    assert( omptm_event.parallel_begin == omptm );

    // into parallel
    reset_event_checkpoints();
    callbacks.implicit_task(ompt_scope_begin, &parallel_data_l1, &task_data, 1,
            0, ompt_task_implicit);
    assert( talp_event.into_parallel_function  == talp );
    assert( omptm_event.into_parallel_function == omptm );
    assert( task_data.value == 1 ); /* nesting level */

    // parallel begin (level 2)
    reset_event_checkpoints();
    callbacks.parallel_begin(&task_data, &nested_encountering_task_frame,
            &parallel_data_l2, requested_parallelism, ompt_parallel_team, codeptr_ra);
    assert( talp_event.parallel_begin  == talp );
    assert( omptm_event.parallel_begin == omptm );

    // into parallel (level 2)
    reset_event_checkpoints();
    callbacks.implicit_task(ompt_scope_begin, &parallel_data_l2,
            &new_task_data, 1, 0, ompt_task_implicit);
    assert( talp_event.into_parallel_function  == talp );
    assert( omptm_event.into_parallel_function == omptm );
    assert( new_task_data.value == 2 ); /* nesting level */

    // into parallel implicit barrier (level 2)
    reset_event_checkpoints();
    callbacks.sync_region(ompt_sync_region_barrier_implicit, ompt_scope_begin,
            &parallel_data_l2, &new_task_data, codeptr_ra);
    assert( talp_event.into_parallel_implicit_barrier  == talp );
    assert( omptm_event.into_parallel_implicit_barrier == omptm );

    // parallel end (level 2)
    reset_event_checkpoints();
    callbacks.parallel_end(&parallel_data_l2, &new_task_data,
            ompt_parallel_team, codeptr_ra);
    assert( talp_event.parallel_end  == talp );
    assert( omptm_event.parallel_end == omptm );

    // into parallel implicit barrier
    reset_event_checkpoints();
    callbacks.sync_region(ompt_sync_region_barrier_implicit, ompt_scope_begin,
            &parallel_data_l1, &task_data, codeptr_ra);
    assert( talp_event.into_parallel_implicit_barrier  == talp );
    assert( omptm_event.into_parallel_implicit_barrier == omptm );

    // into parallel implicit barrier (using new enum)
    reset_event_checkpoints();
    callbacks.sync_region(ompt_sync_region_barrier_implicit_parallel, ompt_scope_begin,
            &parallel_data_l1, &task_data, codeptr_ra);
    assert( talp_event.into_parallel_implicit_barrier  == talp );
    assert( omptm_event.into_parallel_implicit_barrier == omptm );

    // parallel end
    reset_event_checkpoints();
    callbacks.parallel_end(&parallel_data_l1, &encountering_task_data,
            ompt_parallel_team, codeptr_ra);
    assert( talp_event.parallel_end  == talp );
    assert( omptm_event.parallel_end == omptm );

    // task create
    reset_event_checkpoints();
    callbacks.task_create(&encountering_task_data, &encountering_task_frame,
            &new_task_data, ompt_task_explicit, 0, codeptr_ra);
    assert( talp_event.task_create  == talp );
    assert( omptm_event.task_create == omptm );

    // task complete
    reset_event_checkpoints();
    callbacks.task_schedule(&task_data, ompt_task_complete, &new_task_data);
    assert( talp_event.task_complete  == talp );
    assert( omptm_event.task_complete == omptm );

    // task switch
    reset_event_checkpoints();
    callbacks.task_schedule(&task_data, ompt_task_switch, &new_task_data);
    assert( talp_event.task_switch  == talp );
    assert( omptm_event.task_switch == omptm );

    // finalize
    tool_result->finalize(&tool_data);
    assert( callbacks.thread_begin == NULL );
    assert( callbacks.thread_end == NULL );
    assert( callbacks.thread_role_shift == NULL );
    assert( callbacks.parallel_begin == NULL );
    assert( callbacks.parallel_end == NULL );
    assert( callbacks.task_create == NULL );
    assert( callbacks.task_schedule == NULL );
    assert( callbacks.implicit_task == NULL );
    assert( callbacks.sync_region == NULL );
}

int main (int argc, char *argv[]) {

    omptool_event_funcs_t talp_test_funcs = {
        .init = talp_event_init,
        .finalize = talp_event_finalize,
        .into_mpi = talp_event_into_mpi,
        .outof_mpi = talp_event_outof_mpi,
        .lend_from_api = talp_event_lend_from_api,
        .thread_begin = talp_event_thread_begin,
        .thread_end = talp_event_thread_end,
        .thread_role_shift = talp_event_thread_role_shift,
        .parallel_begin = talp_event_parallel_begin,
        .parallel_end = talp_event_parallel_end,
        .into_parallel_function = talp_event_into_parallel_function,
        .into_parallel_implicit_barrier = talp_event_into_parallell_implicit_barrier,
        .task_create = talp_event_task_create,
        .task_complete = talp_event_task_complete,
        .task_switch = talp_event_task_switch,
    };
    omptool_event_funcs_t omptm_test_funcs = {
        .init = omptm_event_init,
        .finalize = omptm_event_finalize,
        .into_mpi = omptm_event_into_mpi,
        .outof_mpi = omptm_event_outof_mpi,
        .lend_from_api = omptm_event_lend_from_api,
        .thread_begin = omptm_event_thread_begin,
        .thread_end = omptm_event_thread_end,
        .thread_role_shift = omptm_event_thread_role_shift,
        .parallel_begin = omptm_event_parallel_begin,
        .parallel_end = omptm_event_parallel_end,
        .into_parallel_function = omptm_event_into_parallel_function,
        .into_parallel_implicit_barrier = omptm_event_into_parallell_implicit_barrier,
        .task_create = omptm_event_task_create,
        .task_complete = omptm_event_task_complete,
        .task_switch = omptm_event_task_switch,
    };
    omptool_testing__setup_event_fn_ptrs(&talp_test_funcs, &omptm_test_funcs);

    /* Options */
    char options[64] = "--ompt --shm-key=";
    strcat(options, SHMEM_KEY);

    /* Modify DLB_ARGS to include options */
    dlb_setenv("DLB_ARGS", options, NULL, ENV_APPEND);

    /* Save initial DLB_ARGS */
    const char *dlb_args_env = getenv("DLB_ARGS");
    size_t len = strlen(dlb_args_env) + 1;
    char *dlb_args = malloc(sizeof(char*)*len);
    strcpy(dlb_args, dlb_args_env);

    /* Obtain OMPT functions */
    ompt_start_tool_result_t *tool_result = ompt_start_tool(1, "Fake OpenMP runtime");
    assert( tool_result );
    assert( tool_result->initialize );
    assert( tool_result->finalize );

    /* Initialize OMPT tool w/o callbacks */
    test_omptool_events(tool_result, /* talp: */ false, /* omptm: */ false);

    /* Initialize OMPT tool with only OMPTM callbacks */
    dlb_setenv("DLB_ARGS", dlb_args, NULL, ENV_OVERWRITE_ALWAYS);
    dlb_setenv("DLB_ARGS", "--lewi", NULL, ENV_APPEND);
    test_omptool_events(tool_result, /* talp: */ false, /* omptm: */ true);

    /* Initialize OMPT tool with only TALP callbacks */
    dlb_setenv("DLB_ARGS", dlb_args, NULL, ENV_OVERWRITE_ALWAYS);
    dlb_setenv("DLB_ARGS", "--talp-openmp", NULL, ENV_APPEND);
    test_omptool_events(tool_result, /* talp: */ true, /* omptm: */ false);

    /* Initialize OMPT tool with both callbacks */
    dlb_setenv("DLB_ARGS", dlb_args, NULL, ENV_OVERWRITE_ALWAYS);
    dlb_setenv("DLB_ARGS", "--lewi --talp-openmp", NULL, ENV_APPEND);
    test_omptool_events(tool_result, /* talp: */ true, /* omptm: */ true);

    return 0;
}
