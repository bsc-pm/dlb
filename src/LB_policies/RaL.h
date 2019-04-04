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

#ifndef RAL_H
#define RAL_H

/******* Main Functions - LeWI Mask Balancing Policy ********/

void RaL_Init(const cpu_set_t* process_mask);

void RaL_Finish( void );

void RaL_IntoCommunication( void );

void RaL_OutOfCommunication( void );

void RaL_IntoBlockingCall(int is_iter, int blocking_mode);

void RaL_OutOfBlockingCall(int is_iter);

void RaL_UpdateResources( int max_resources );

void RaL_ReturnClaimedCpus( void );

#endif /* RAL_H */

