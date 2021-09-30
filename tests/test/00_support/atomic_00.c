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

#include "support/atomic.h"

/* #include <stdio.h> */
/* #include <unistd.h> */
#include <assert.h>

int main(int argc, char *argv[]) {
    /* int err; */

    atomic_int num = 0;

    assert( DLB_ATOMIC_ADD(&num, 1) == 0 );             // num = 1
    assert( DLB_ATOMIC_ADD_RLX(&num, 1) == 1 );         // num = 2
    assert( DLB_ATOMIC_ADD_FETCH(&num, 1) == 3 );       // num = 3
    assert( DLB_ATOMIC_ADD_FETCH_RLX(&num, 1) == 4 );   // num = 4

    assert( DLB_ATOMIC_SUB(&num, 1) == 4 );             // num = 3
    assert( DLB_ATOMIC_SUB_RLX(&num, 1) == 3 );         // num = 2
    assert( DLB_ATOMIC_SUB_FETCH(&num, 1) == 1 );       // num = 1
    assert( DLB_ATOMIC_SUB_FETCH_RLX(&num, 1) == 0 );   // num = 0

    DLB_ATOMIC_ST(&num, 5);         assert( num == 5);
    DLB_ATOMIC_ST_RLX(&num, 6);     assert( num == 6);
    DLB_ATOMIC_ST_REL(&num, 7);     assert( num == 7);

    int val;
    val = DLB_ATOMIC_LD(&num);      assert( num == val);
    val = DLB_ATOMIC_LD_RLX(&num);  assert( num == val);
    val = DLB_ATOMIC_LD_ACQ(&num);  assert( num == val);

    return 0;
}
