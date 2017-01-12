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
    test_generator="gens/basic-generator"
    test_generator_ENV=( "LB_TEST_MODE=single" )
</testinfo>*/

#include <stdio.h>
#include <stdlib.h>
#include "support/types.h"
#include "support/options.h"

int main(int argc, char *argv[]) {
    verbose_opts_t vb_opts;
    verbose_fmt_t vbf_opts;
    int error = 0;

    parse_verbose_opts("api", &vb_opts);
    if (vb_opts != VB_API) {
        error++;
        fprintf(stderr, "Simple verbose option failed\n");
    }

    parse_verbose_opts("shmem:mpi_api:drom", &vb_opts);
    if (vb_opts != (VB_SHMEM | VB_MPI_API | VB_DROM)) {
        error++;
        fprintf(stderr, "Multiple verbose option failed\n");
    }

    parse_verbose_fmt("mpinode", &vbf_opts);
    if (vbf_opts != VBF_MPINODE) {
        error++;
        fprintf(stderr, "Simple verbose format option failed\n");
    }

    parse_verbose_fmt("node:pid:thread", &vbf_opts);
    if (vbf_opts != (VBF_NODE | VBF_PID | VBF_THREAD)) {
        error++;
        fprintf(stderr, "Multiple verbose format option failed\n");
    }

    // Check DLB defaults
    options_init();
    vb_opts = options_get_verbose();
    if (vb_opts != VB_CLEAR) {
        error++;
        fprintf(stderr, "Default verbose option failed\n");
    }

    vbf_opts = options_get_verbose_fmt();
    if (vbf_opts != (VBF_NODE | VBF_PID | VBF_THREAD)) {
        error++;
        fprintf(stderr, "Default verbose format option failed\n");
    }
    options_finalize();

    return error;
}
