/*************************************************************************************/
/*      Copyright 2014 Barcelona Supercomputing Center                               */
/*                                                                                   */
/*      This file is part of the DLB library.                                        */
/*                                                                                   */
/*      DLB is free software: you can redistribute it and/or modify                  */
/*      it under the terms of the GNU Lesser General Public License as published by  */
/*      the Free Software Foundation, either version 3 of the License, or            */
/*      (at your option) any later version.                                          */
/*                                                                                   */
/*      DLB is distributed in the hope that it will be useful,                       */
/*      but WITHOUT ANY WARRANTY; without even the implied warranty of               */
/*      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                */
/*      GNU Lesser General Public License for more details.                          */
/*                                                                                   */
/*      You should have received a copy of the GNU Lesser General Public License     */
/*      along with DLB.  If not, see <http://www.gnu.org/licenses/>.                 */
/*************************************************************************************/

/*<testinfo>
    test_generator="gens/basic-generator"
    test_generator_ENV=( "LB_TEST_MODE=single" )
</testinfo>*/

#include <stdio.h>
#include <stdlib.h>
#include "support/utils.h"
#include "support/mask_utils.h"

int main( int argc, char **argv ) {
    mu_init();
    fprintf( stdout, "System size: %d\n", mu_get_system_size() );

    setenv("LB_MASK", "0-1,3,5-7", 1);

    cpu_set_t lb_mask;
    parse_env_cpuset( "LB_MASK", &lb_mask );
    fprintf( stdout, "LB Mask: %s\n", mu_to_str(&lb_mask) );

    mu_finalize();
    return 0;
}
