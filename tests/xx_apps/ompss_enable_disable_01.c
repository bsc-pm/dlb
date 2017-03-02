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
    compile_versions="nanox_ompss"

    test_CC_nanox_ompss="smpcc"
    test_CFLAGS_nanox_ompss="--ompss"

    test_generator="gens/basic-generator"
    test_generator_ENV=( LB_TEST_POLICY="auto_LeWI_mask" )
    test_ENV=( NX_ARGS="--thread-manager=dlb" )
</testinfo>*/

#include <stdio.h>
#include <omp.h>
#include <nanos.h>
#include <DLB_interface.h>

int main()
{
    int i;
    int N = 8;

    DLB_disable();

    #pragma omp for schedule(static)
    for(i=0; i<N; ++i) {
        printf("Thread %d, running wd %d\n",
                omp_get_thread_num(),
                nanos_get_wd_id(nanos_current_wd()));
    }

    DLB_enable();

    #pragma omp for schedule(dynamic)
    for(i=0; i<N; ++i) {
        printf("Thread %d, running wd %d\n",
                omp_get_thread_num(),
                nanos_get_wd_id(nanos_current_wd()));
    }

    return 0;
}
