/*********************************************************************************/
/*  Copyright 2009-2025 Barcelona Supercomputing Center                          */
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

#ifndef EXTRA_TESTS_T
#define EXTRA_TESTS_T

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* Set the environment variable DLB_EXTRA_TESTS=1 to enable the execution of
 * additional CPU-intensive or fork-intensive tests */

#define DLB_EXTRA_TESTS dlb_extra_tests()

static inline bool dlb_extra_tests(void) {
    const char *env = getenv("DLB_EXTRA_TESTS");
    return env && env[0] == '1';
}

#endif /* EXTRA_TESTS_T */
