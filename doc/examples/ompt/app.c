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

#include <stdio.h>
#include <dlb.h>

int main()
{
    #pragma omp parallel
    {}

    int err = DLB_Init(0,0,0);
    switch (err) {
        case DLB_SUCCESS:
            printf("DLB has not been initialized through OpenMP.\n"
                    "Make sure that the OpenMP runtime supports OMPT and that"
                    " DLB_ARGS is correctly set.\n");
            DLB_Finalize();
            break;
        case DLB_ERR_INIT:
            printf("DLB has correctly been registered as an OpenMP Tool.\n");
            break;
        default:
            printf("Unknown error, please report.");
            return 1;
    }

    return 0;
}
