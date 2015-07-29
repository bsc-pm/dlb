/*********************************************************************************/
/*  Copyright 2015 Barcelona Supercomputing Center                               */
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
    compile_versions="gomp nanox_omp nanox_ompss"

    test_CC_gomp="gcc"
    test_CFLAGS_gomp="-fopenmp"
    test_LDFLAGS_gomp="-Wl,--no-as-needed"

    test_CC_nanox_omp="smpcc"
    test_CFLAGS_nanox_omp="--openmp"

    test_CC_nanox_ompss="smpcc"
    test_CFLAGS_nanox_ompss="--ompss"

    test_generator="gens/basic-generator"
    test_generator_ENV=( "LB_TEST_MODE=single" )
    test_ENV=( "LB_DROM=1" )
</testinfo>*/

#include "LB_core/DLB_interface.h"

int main( int argc, char **argv ) {
    // DLB_Update should not fail even if called before DLB_Drom_Init
    DLB_Update();

    DLB_Drom_Init();
    DLB_Update();

    DLB_Init();
    DLB_Update();
    DLB_Finalize();

    return 0;
}
