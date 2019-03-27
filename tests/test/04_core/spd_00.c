/*********************************************************************************/
/*  Copyright 2009-2018 Barcelona Supercomputing Center                          */
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

#include "LB_core/spd.h"

#include <pthread.h>
#include <assert.h>

int main(int argc, char *argv[]) {

    subprocess_descriptor_t spd1, spd2;
    spd1.id = 111;
    spd2.id = 222;

    spd_register(&spd1);
    spd_unregister(&spd1);
    spd_register(&spd2);
    spd_unregister(&spd2);

    spd_register(&spd1);
    spd_register(&spd2);

    const subprocess_descriptor_t **spds = spd_get_spds();
    /* spds comes from a gtree, order is undetermined */
    assert( (spds[0]->id == spd1.id && spds[1]->id == spd2.id)
            || (spds[0]->id == spd2.id && spds[1]->id == spd1.id) );
    assert( spds[2] == NULL );
    free(spds);

    pthread_t pthread = pthread_self();
    spd_set_pthread(&spd1, pthread);
    assert( spd_get_pthread(&spd1) == pthread );
    assert( spd_get_pthread(&spd2) == 0 );
    spd_set_pthread(&spd2, pthread);
    assert( spd_get_pthread(&spd2) == pthread );

    spd_unregister(&spd2);
    spd_unregister(&spd1);

    return 0;
}
