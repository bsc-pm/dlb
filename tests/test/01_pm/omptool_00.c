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

#include "LB_numThreads/omp-tools.h"
#include "LB_numThreads/omptool.h"
#include "apis/dlb_errors.h"

#include <sched.h>
#include <string.h>
#include <assert.h>

ompt_start_tool_result_t* ompt_start_tool(
        unsigned int omp_version, const char *runtime_version);


static void* lookup_fn (const char *interface_function_name) {
    return NULL;
}

int main (int argc, char *argv[]) {
    /* Initialize OMPT tool */
    ompt_start_tool_result_t *tool_result = ompt_start_tool(1, "Fake OpenMP runtime");
    assert( tool_result );
    assert( tool_result->initialize );
    assert( tool_result->finalize );
    tool_result->initialize((ompt_function_lookup_t)lookup_fn, 0, NULL);

    return 0;
}
