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
#include <string.h>
#include <assert.h>

int nthreads = 0;
cpu_set_t process_mask;

void cb_set_num_threads(int num_threads) { nthreads = num_threads; }
void cb_set_active_mask(const cpu_set_t *mask) {}
void cb_set_process_mask(const cpu_set_t *mask) { memcpy(&process_mask, mask, sizeof(cpu_set_t)); }
void cb_add_active_mask(const cpu_set_t *mask) {}
void cb_add_process_mask(const cpu_set_t *mask) {}
void cb_enable_cpu(int cpuid) { CPU_SET(cpuid, &process_mask); }
void cb_disable_cpu(int cpuid) { CPU_CLR(cpuid, &process_mask); }

int main( int argc, char **argv ) {
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_ZERO(&process_mask);
    pm_interface_t pm;
    pm_init(&pm);

    // Call callbacks without initialization and check DLB_ERR_NOCBK
    assert( update_threads(&pm, 2) == DLB_ERR_NOCBK );
    assert( set_mask(&pm, &mask) == DLB_ERR_NOCBK );
    assert( set_process_mask(&pm, &mask) == DLB_ERR_NOCBK );
    assert( add_mask(&pm, &mask) == DLB_ERR_NOCBK );
    assert( add_process_mask(&pm, &mask) == DLB_ERR_NOCBK );
    assert( enable_cpu(&pm, 0) == DLB_ERR_NOCBK );
    assert( disable_cpu(&pm, 0) == DLB_ERR_NOCBK );

    // Set callbacks
    assert( pm_callback_set(&pm, dlb_callback_set_num_threads, (dlb_callback_t)cb_set_num_threads)
            == DLB_SUCCESS );
    assert( pm_callback_set(&pm, dlb_callback_set_active_mask, (dlb_callback_t)cb_set_active_mask)
            == DLB_SUCCESS );
    assert( pm_callback_set(&pm, dlb_callback_set_process_mask, (dlb_callback_t)cb_set_process_mask)
            == DLB_SUCCESS );
    assert( pm_callback_set(&pm, dlb_callback_add_active_mask, (dlb_callback_t)cb_add_active_mask)
            == DLB_SUCCESS );
    assert( pm_callback_set(&pm, dlb_callback_add_process_mask, (dlb_callback_t)cb_add_process_mask)
            == DLB_SUCCESS );
    assert( pm_callback_set(&pm, dlb_callback_enable_cpu, (dlb_callback_t)cb_enable_cpu)
            == DLB_SUCCESS );
    assert( pm_callback_set(&pm, dlb_callback_disable_cpu, (dlb_callback_t)cb_disable_cpu)
            == DLB_SUCCESS );
    assert( pm_callback_set(&pm, 42, (dlb_callback_t)main)
            == DLB_ERR_NOCBK );

    // Get callbacks
    dlb_callback_t cb;
    assert( pm_callback_get(&pm, dlb_callback_set_num_threads, &cb) == DLB_SUCCESS );
    assert( cb == (dlb_callback_t)cb_set_num_threads );
    assert( pm_callback_get(&pm, dlb_callback_set_active_mask, &cb) == DLB_SUCCESS );
    assert( cb == (dlb_callback_t)cb_set_active_mask );
    assert( pm_callback_get(&pm, dlb_callback_set_process_mask, &cb) == DLB_SUCCESS );
    assert( cb == (dlb_callback_t)cb_set_process_mask );
    assert( pm_callback_get(&pm, dlb_callback_add_active_mask, &cb) == DLB_SUCCESS );
    assert( cb == (dlb_callback_t)cb_add_active_mask );
    assert( pm_callback_get(&pm, dlb_callback_add_process_mask, &cb) == DLB_SUCCESS );
    assert( cb == (dlb_callback_t)cb_add_process_mask );
    assert( pm_callback_get(&pm, dlb_callback_enable_cpu, &cb) == DLB_SUCCESS );
    assert( cb == (dlb_callback_t)cb_enable_cpu );
    assert( pm_callback_get(&pm, dlb_callback_disable_cpu, &cb) == DLB_SUCCESS );
    assert( cb == (dlb_callback_t)cb_disable_cpu );
    assert( pm_callback_get(&pm, 42, &cb) == DLB_ERR_NOCBK );

    // Call callback and check DLB_SUCCESS
    assert( update_threads(&pm, 0) == DLB_SUCCESS );
    assert( update_threads(&pm, 2) == DLB_SUCCESS );
    assert( update_threads(&pm, 42) == DLB_SUCCESS );
    assert( set_mask(&pm, &mask) == DLB_SUCCESS );
    assert( set_process_mask(&pm, &mask) == DLB_SUCCESS );
    assert( add_mask(&pm, &mask) == DLB_SUCCESS );
    assert( add_process_mask(&pm, &mask) == DLB_SUCCESS );
    assert( enable_cpu(&pm, 0) == DLB_SUCCESS );
    assert( disable_cpu(&pm, 0) == DLB_SUCCESS );

    // Call callback and check that the parameter is correct
    update_threads(&pm, 1);
    assert(nthreads == 1);
    CPU_ZERO(&mask);
    CPU_SET(0, &mask);
    CPU_SET(1, &mask);
    assert( set_process_mask(&pm, &mask) == DLB_SUCCESS );
    assert( CPU_COUNT(&process_mask) == 2 );
    assert( enable_cpu(&pm, 2) == DLB_SUCCESS );
    assert( CPU_COUNT(&process_mask) == 3 );
    assert( disable_cpu(&pm, 2) == DLB_SUCCESS );
    assert( CPU_COUNT(&process_mask) == 2 );


    return 0;
}
