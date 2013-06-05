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

#ifndef RAL_H
#define RAL_H

/******* Main Functions - LeWI Mask Balancing Policy ********/

void RaL_init( /* TODO: should be void */ );

void RaL_finish( void );

void RaL_init_iteration( void );

void RaL_finish_iteration( void);

void RaL_into_communication( void );

void RaL_out_of_communication( void );

void RaL_into_blocking_call( void);

void RaL_out_of_blocking_call( void );

void RaL_update_resources( int max_resources );

#endif /* RAL_H */

