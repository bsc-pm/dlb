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

#ifndef DLB_INTERFACE_H
#define DLB_INTERFACE_H

#define DLB_API_SIMPLE_DECL(Type, Name, Params) \
   extern Type Name##_ Params; \
   extern Type Name Params;

#define DLB_API_DECL(Type, Name, NameF, Params) \
   DLB_API_SIMPLE_DECL(Type, NameF, Params) \
   DLB_API_SIMPLE_DECL(Type, Name, Params)

#define DLB_API_SIMPLE_DEF(Type, Name, Params) \
   __attribute__((alias(#Name))) Type Name##_ Params; \
   Type Name Params

#define DLB_API_DEF(Type, Name, NameF, Params) \
   __attribute__((alias(#Name))) Type NameF Params; \
   __attribute__((alias(#Name))) Type NameF##_ Params; \
   DLB_API_SIMPLE_DEF(Type, Name, Params)

DLB_API_DECL( void, DLB_enable, dlb_enable, (void) );
DLB_API_DECL( void, DLB_disable, dlb_disable, (void) );
DLB_API_DECL( void, DLB_single, dlb_single, (void) );
DLB_API_DECL( void, DLB_parallel, dlb_parallel, (void) );
DLB_API_DECL( void, DLB_UpdateResources, dlb_updateresources, (int max_resources) );
DLB_API_DECL( void, DLB_Lend, dlb_lend, (void) );
DLB_API_DECL( void, DLB_Retrieve, dlb_retrieve, (void) );
DLB_API_DECL( void, DLB_Barrier, dlb_barrier, (void) );

#endif /* DLB_INTERFACE_H */
