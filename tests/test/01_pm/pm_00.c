/*********************************************************************************/
/*  Copyright 2017 Barcelona Supercomputing Center                               */
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
/*  along with DLB.  If not, see <http://www.gnu.org/licenses/>.                 */
/*********************************************************************************/

/*<testinfo>
    test_generator="gens/basic-generator"
    test_generator_ENV=( "LB_TEST_MODE=single" )
</testinfo>*/

#include "LB_numThreads/numThreads.h"
#include "apis/dlb_errors.h"

#include <sched.h>
#include <assert.h>

void cb_set_num_threads(int num_threads) {}
void cb_set_active_mask(const cpu_set_t *mask) {}
void cb_set_process_mask(const cpu_set_t *mask) {}
void cb_add_active_mask(const cpu_set_t *mask) {}
void cb_add_process_mask(const cpu_set_t *mask) {}
void cb_enable_cpu(int cpuid) {}
void cb_disable_cpu(int cpuid) {}

int main( int argc, char **argv ) {
    int error;
    cpu_set_t mask;
    CPU_ZERO(&mask);
    pm_interface_t pm;
    pm_init(&pm);

    // Call callbacks without initialization and check DLB_ERR_NOCBK
    error = update_threads(&pm, 2);
    assert(error == DLB_ERR_NOCBK);
    error = set_mask(&pm, &mask);
    assert(error == DLB_ERR_NOCBK);
    error = set_process_mask(&pm, &mask);
    assert(error == DLB_ERR_NOCBK);
    error = add_mask(&pm, &mask);
    assert(error == DLB_ERR_NOCBK);
    error = add_process_mask(&pm, &mask);
    assert(error == DLB_ERR_NOCBK);
    error = enable_cpu(&pm, 0);
    assert(error == DLB_ERR_NOCBK);
    error = disable_cpu(&pm, 0);
    assert(error == DLB_ERR_NOCBK);

    // Set callbacks
    pm_callback_set(&pm, dlb_callback_set_num_threads, (dlb_callback_t)cb_set_num_threads);
    pm_callback_set(&pm, dlb_callback_set_active_mask, (dlb_callback_t)cb_set_active_mask);
    pm_callback_set(&pm, dlb_callback_set_process_mask, (dlb_callback_t)cb_set_process_mask);
    pm_callback_set(&pm, dlb_callback_add_active_mask, (dlb_callback_t)cb_add_active_mask);
    pm_callback_set(&pm, dlb_callback_add_process_mask, (dlb_callback_t)cb_add_process_mask);
    pm_callback_set(&pm, dlb_callback_enable_cpu, (dlb_callback_t)cb_enable_cpu);
    pm_callback_set(&pm, dlb_callback_disable_cpu, (dlb_callback_t)cb_disable_cpu);
    error = pm_callback_set(&pm, 42, (dlb_callback_t)main);
    assert(error == DLB_ERR_NOCBK);

    // Get callbacks
    dlb_callback_t cb;
    pm_callback_get(&pm, dlb_callback_set_num_threads, &cb);
    assert(cb==(dlb_callback_t)cb_set_num_threads);
    pm_callback_get(&pm, dlb_callback_set_active_mask, &cb);
    assert(cb==(dlb_callback_t)cb_set_active_mask);
    pm_callback_get(&pm, dlb_callback_set_process_mask, &cb);
    assert(cb==(dlb_callback_t)cb_set_process_mask);
    pm_callback_get(&pm, dlb_callback_add_active_mask, &cb);
    assert(cb==(dlb_callback_t)cb_add_active_mask);
    pm_callback_get(&pm, dlb_callback_add_process_mask, &cb);
    assert(cb==(dlb_callback_t)cb_add_process_mask);
    pm_callback_get(&pm, dlb_callback_enable_cpu, &cb);
    assert(cb==(dlb_callback_t)cb_enable_cpu);
    pm_callback_get(&pm, dlb_callback_disable_cpu, &cb);
    assert(cb==(dlb_callback_t)cb_disable_cpu);
    error = pm_callback_get(&pm, 42, &cb);
    assert(error == DLB_ERR_NOCBK);

    // Call callback and check DLB_SUCCESS
    error = update_threads(&pm, 0);
    assert(error == DLB_SUCCESS);
    error = update_threads(&pm, 2);
    assert(error == DLB_SUCCESS);
    error = update_threads(&pm, 42);
    assert(error == DLB_SUCCESS);
    error = set_mask(&pm, &mask);
    assert(error == DLB_SUCCESS);
    error = set_process_mask(&pm, &mask);
    assert(error == DLB_SUCCESS);
    error = add_mask(&pm, &mask);
    assert(error == DLB_SUCCESS);
    error = add_process_mask(&pm, &mask);
    assert(error == DLB_SUCCESS);
    error = enable_cpu(&pm, 0);
    assert(error == DLB_SUCCESS);
    error = disable_cpu(&pm, 0);
    assert(error == DLB_SUCCESS);

    return 0;
}
