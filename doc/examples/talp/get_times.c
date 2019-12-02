/*********************************************************************************/
/*  Copyright 2009-2019 Barcelona Supercomputing Center                          */
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

#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlb.h>
#include <dlb_talp.h>

int main( int argc, char **argv )
{
    if ( argc!=2 ) {
        fprintf( stderr, "Usage: %s <pid>\n", argv[0] );
        return EXIT_FAILURE;
    }
    int pid = atoi( argv[1] );
    int error;
    double mpi_time,cpu_time;

    DLB_TALP_Attach();

    while( !kill(pid, 0) ) {
        error = DLB_TALP_MPITimeGet(pid, &mpi_time);
        if (error != DLB_SUCCESS) break;
        error = DLB_TALP_CPUTimeGet(pid, &cpu_time);
        if (error != DLB_SUCCESS) break;

        fprintf( stdout, "\n\033[F\033[J" );
        fprintf( stdout, "%d, mpi time: %g", pid, mpi_time );
        fprintf( stdout, "; cpu time: %g",  cpu_time );
        usleep( 500000 );
    }

    DLB_TALP_Detach();

    return EXIT_SUCCESS;
}
