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

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlb_stats.h>

#define ARRAY_SIZE 64

int main( int argc, char **argv )
{
    DLB_Stats_Init();

    int pidlist[ARRAY_SIZE];
    int nelems;
    DLB_Stats_GetPidList( pidlist, &nelems, ARRAY_SIZE );

    printf( "Nelems is %d\n", nelems );
    int i;
    for (i=0; i<nelems; i++) {
        printf( "pidlist[%d] is %d\n", i, pidlist[i] );
    }

    DLB_Stats_Finalize();

    return EXIT_SUCCESS;
}
