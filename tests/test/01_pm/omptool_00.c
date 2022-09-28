/*********************************************************************************/
/*  Copyright 2009-2021 Barcelona Supercomputing Center                          */
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



static bool talp_cb_thread_begin_called = false;
static void talp_cb_thread_begin(ompt_thread_t thread_type, ompt_data_t *thread_data) {
    talp_cb_thread_begin_called = true;
}

static bool omptm_cb_thread_begin_called = false;
static void omptm_cb_thread_begin(ompt_thread_t thread_type, ompt_data_t *thread_data) {
    omptm_cb_thread_begin_called = true;
}

static funcs_t callbacks = {0};
static ompt_set_result_t set_callback(ompt_callbacks_t event, ompt_callback_t callback) {
    switch(event) {
        case ompt_callback_thread_begin:
            callbacks.thread_begin = (ompt_callback_thread_begin_t)callback;
            break;
        default:
            break;
    }
    return ompt_set_always;
}

static void* lookup_fn (const char *interface_function_name) {
    if (strcmp(interface_function_name, "ompt_set_callback") == 0) {
            return set_callback;
    }
    return NULL;
}

int main (int argc, char *argv[]) {

    funcs_t talp_test_funcs = {0};
    funcs_t omptm_test_funcs = {0};
    ompt_data_t thread_data;

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

    /* Initialize OMPT tool w/o callbacks (assuming  --ompt-thread-manager=none)*/
    tool_result->initialize((ompt_function_lookup_t)lookup_fn, 0, NULL);
    assert( callbacks.thread_begin == NULL );
    tool_result->finalize(NULL);

    /* Initialize OMPT tool with only OMPTM callbacks */
    talp_cb_thread_begin_called = false;
    omptm_cb_thread_begin_called = false;
    callbacks.thread_begin = NULL;
    talp_test_funcs = (const funcs_t) {};
    omptm_test_funcs = (const funcs_t) {.thread_begin = omptm_cb_thread_begin};
    dlb_setenv("DLB_ARGS", dlb_args, NULL, ENV_OVERWRITE_ALWAYS);
    dlb_setenv("DLB_ARGS", "--ompt-thread-manager=omp5", NULL, ENV_APPEND);
    omptool_testing__setup_omp_fn_ptrs(&talp_test_funcs, &omptm_test_funcs);
    tool_result->initialize((ompt_function_lookup_t)lookup_fn, 0, NULL);
    assert( callbacks.thread_begin != NULL );
    assert( omptm_cb_thread_begin_called == false );
    callbacks.thread_begin(ompt_thread_initial, &thread_data);
    assert( omptm_cb_thread_begin_called == true );
    assert( talp_cb_thread_begin_called == false );
    tool_result->finalize(NULL);

    /* Initialize OMPT tool with only TALP callbacks */
    talp_cb_thread_begin_called = false;
    omptm_cb_thread_begin_called = false;
    callbacks.thread_begin = NULL;
    talp_test_funcs = (const funcs_t) {.thread_begin = talp_cb_thread_begin};
    omptm_test_funcs = (const funcs_t) {};
    dlb_setenv("DLB_ARGS", dlb_args, NULL, ENV_OVERWRITE_ALWAYS);
    dlb_setenv("DLB_ARGS", "--talp-openmp", NULL, ENV_APPEND);
    omptool_testing__setup_omp_fn_ptrs(&talp_test_funcs, &omptm_test_funcs);
    tool_result->initialize((ompt_function_lookup_t)lookup_fn, 0, NULL);
    assert( callbacks.thread_begin != NULL );
    assert( talp_cb_thread_begin_called == false );
    callbacks.thread_begin(ompt_thread_initial, &thread_data);
    assert( talp_cb_thread_begin_called == true );
    assert( omptm_cb_thread_begin_called == false );
    tool_result->finalize(NULL);

    /* Initialize OMPT tool with both callbacks */
    talp_cb_thread_begin_called = false;
    omptm_cb_thread_begin_called = false;
    callbacks.thread_begin = NULL;
    talp_test_funcs = (const funcs_t) {.thread_begin = talp_cb_thread_begin};
    omptm_test_funcs = (const funcs_t) {.thread_begin = omptm_cb_thread_begin};
    dlb_setenv("DLB_ARGS", dlb_args, NULL, ENV_OVERWRITE_ALWAYS);
    dlb_setenv("DLB_ARGS", "--ompt-thread-manager=omp5 --talp-openmp", NULL, ENV_APPEND);
    omptool_testing__setup_omp_fn_ptrs(&talp_test_funcs, &omptm_test_funcs);
    tool_result->initialize((ompt_function_lookup_t)lookup_fn, 0, NULL);
    assert( callbacks.thread_begin != NULL );
    assert( talp_cb_thread_begin_called == false );
    assert( omptm_cb_thread_begin_called == false );
    callbacks.thread_begin(ompt_thread_initial, &thread_data);
    assert( talp_cb_thread_begin_called == true );
    assert( omptm_cb_thread_begin_called == true );
    tool_result->finalize(NULL);

    return 0;
}
