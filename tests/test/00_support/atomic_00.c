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

#include <stdbool.h>
#include <assert.h>

int main(int argc, char *argv[]) {

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

    /* Test atomic bit functions */
    {
        typedef enum MyFlags {
            CLEAR = 0,
            FLAG0 = 1 << 0,
            FLAG1 = 1 << 1,
            FLAG2 = 1 << 2,
            FLAG3 = 1 << 3,
        } flags_t;
        bool b;
        _Atomic(flags_t) flags;
        atomic_int *_flags = (atomic_int*)&flags;

        /* set_bit */
        flags = FLAG0;
        b = set_bit(_flags, FLAG1);
        assert( b == true && flags == (FLAG0|FLAG1) );
        b = set_bit(_flags, FLAG1);
        assert( b == false && flags == (FLAG0|FLAG1) );

        /* clear_bit */
        flags = FLAG0|FLAG1;
        b = clear_bit(_flags, FLAG1);
        assert( b == true && flags == FLAG0 );
        b = clear_bit(_flags, FLAG1);
        assert( b == false && flags == FLAG0 );

        /* cas_bit witFLAG1h a single bit */
        flags = FLAG0;
        b = cas_bit(_flags, FLAG0, FLAG1);
        assert( b == true && flags == FLAG1 );
        b = cas_bit(_flags, FLAG0, FLAG2);
        assert( b == false && flags == FLAG1 );

        /* cas_bit with n bits */
        flags = FLAG0|FLAG1;
        b = cas_bit(_flags, FLAG1, FLAG2);
        assert( b == true && flags == (FLAG0|FLAG2) );
        b = cas_bit(_flags, FLAG1, FLAG3);
        assert( b == false && flags == (FLAG0|FLAG2) );
        b = cas_bit(_flags, (FLAG0|FLAG2), (FLAG1|FLAG3));
        assert( b == true && flags == (FLAG1|FLAG3) );
        b = cas_bit(_flags, (FLAG0|FLAG2), (FLAG1|FLAG3));
        assert( b == false && flags == (FLAG1|FLAG3) );

        /* test_set_clear_bit */
        flags = FLAG0;
        b = test_set_clear_bit(_flags, FLAG1, CLEAR);
        assert( b == true && flags == (FLAG0|FLAG1) );
        b = test_set_clear_bit(_flags, FLAG1, FLAG0);
        assert( b == false && flags == (FLAG0|FLAG1) );
        b = test_set_clear_bit(_flags, FLAG2, FLAG1);
        assert( b == true && flags == (FLAG0|FLAG2) );
    }

    return 0;
}
