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

#include "support/debug.h"
#include "support/options.h"

int main( int argc, char **argv ) {
    options_t options;
    options_init(&options, "--verbose-format=node:pid:thread");
    debug_init(&options);
    vb_print(stdout, "DLB TEST", "hello test %s", "!!");

    options_init(&options, "--verbose-format=:");
    debug_init(&options);
    vb_print(stdout, "DLB TEST", "hello test %s", "!!");

    options_init(&options, "--verbose-format=node:");
    debug_init(&options);
    vb_print(stdout, "DLB TEST", "hello test %s", "!!");

    print_backtrace();

    return 0;
}
