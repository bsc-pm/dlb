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

#include "dlb/dlb.h"

void DLB_UpdateResources( int max_resources )
{
   updateresources( max_resources );
}

void dlb_updateresources( int max_resources ) __attribute__ ((alias ("DLB_UpdateResources")));
void DLB_UpdateResources_( int max_resources ) __attribute__ ((alias ("DLB_UpdateResources")));
void dlb_updateresources_( int max_resources ) __attribute__ ((alias ("DLB_UpdateResources")));
