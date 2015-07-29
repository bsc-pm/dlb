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

#ifndef DLB_API_H
#define DLB_API_H

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

#define DLB_API_F_ALIAS( NameF, Params) \
   void NameF##_ Params __attribute__((alias(#NameF))); \
   void NameF##__ Params __attribute__((alias(#NameF)))

#endif /* DLB_API_H */
