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

#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <DLB_interface.h>

int main( int argc, char **argv )
{
    if ( argc!=2 ) {
        fprintf( stderr, "Usage: %s <pid>\n", argv[0] );
        return EXIT_FAILURE;
    }
    int pid = atoi( argv[1] );

    DLB_Stats_Init();

    // while process exists
    while( !kill(pid, 0) ) {
        fprintf( stdout, "\n\033[F\033[J" );
        fprintf( stdout, "%d, Cpu Usage: %g", pid, DLB_Stats_GetCpuUsage(pid) );
        usleep( 500000 );
    }

    DLB_Stats_Finalize();

    return EXIT_SUCCESS;
}
