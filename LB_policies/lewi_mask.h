/*************************************************************************************/
/*      Copyright 2012 Barcelona Supercomputing Center                               */
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

#ifndef LEWI_MASK_H
#define LEWI_MASK_H

/******* Main Functions - LeWI Balancing Policy ********/

void lewi_mask_init(  );

void lewi_mask_finish( void );

void lewi_mask_init_iteration( void );

void lewi_mask_finish_iteration( double cpu_secs, double MPI_secs );

void lewi_mask_into_communication( void );

void lewi_mask_out_of_communication( void );

void lewi_mask_into_blocking_call( double cpu_secs, double MPI_secs );

void lewi_mask_out_of_blocking_call( void );

void lewi_mask_update_resources( void );

#endif /* LEWI_MASK_H */

