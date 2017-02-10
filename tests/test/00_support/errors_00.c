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

#include "apis/dlb_errors.h"
#include "support/error.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>

int main(int argc, char **argv) {

    // Upper bound, error
    fprintf(stderr, "invalid errnum: %d, string: %s\n",
            _DLB_ERROR_UPPER_BOUND, error_get_str(_DLB_ERROR_UPPER_BOUND));
    assert(strcmp(error_get_str(_DLB_ERROR_UPPER_BOUND), "unknown errnum") == 0);

    int i;
    for (i=_DLB_ERROR_UPPER_BOUND-1; i>_DLB_ERROR_LOWER_BOUND; --i) {
        fprintf(stderr, "errnum: %d, string: %s\n", i, error_get_str(i));
    }

    // Lower bound, error
    fprintf(stderr, "invalid errnum: %d, string: %s\n",
            _DLB_ERROR_LOWER_BOUND, error_get_str(_DLB_ERROR_LOWER_BOUND));
    assert(strcmp(error_get_str(_DLB_ERROR_LOWER_BOUND), "unknown errnum") == 0);

    return DLB_SUCCESS;
}
