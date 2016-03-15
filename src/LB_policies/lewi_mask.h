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

#ifndef LEWI_MASK_H
#define LEWI_MASK_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <sched.h>

/******* Main Functions - LeWI Mask Balancing Policy ********/

void lewi_mask_Init( void );

void lewi_mask_Finish( void );

void lewi_mask_IntoCommunication( void );

void lewi_mask_OutOfCommunication( void );

void lewi_mask_IntoBlockingCall(int is_iter, int bloking_mode);

void lewi_mask_OutOfBlockingCall(int is_iter);

void lewi_mask_UpdateResources( int max_resources );

void lewi_mask_ReturnClaimedCpus( void );

void lewi_mask_ClaimCpus( int cpus );

void lewi_mask_acquireCpu( int cpu );

void lewi_mask_acquireCpus(cpu_set_t* cpus);

void lewi_mask_resetDLB( void );

void lewi_mask_disableDLB(void);

void lewi_mask_enableDLB(void);

void lewi_mask_single(void);

void lewi_mask_parallel(void);

#endif /* LEWI_MASK_H */

