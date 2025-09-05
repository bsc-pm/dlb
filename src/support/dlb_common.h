/*********************************************************************************/
/*  Copyright 2009-2025 Barcelona Supercomputing Center                          */
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

#ifndef DLB_COMMON_H
#define DLB_COMMON_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif


#define likely(expr)   __builtin_expect(!!(expr), 1)
#define unlikely(expr) __builtin_expect(!!(expr), 0)


#if !defined(__INTEL_COMPILER) && ((defined(__GNUC__) && __GNUC__ >= 7) || (defined(__clang__) && __clang_major__ >= 11))
#define DLB_FALLTHROUGH __attribute__((__fallthrough__))
#else
#define DLB_FALLTHROUGH ((void)0)
#endif


#define DLB_EXPORT_SYMBOL __attribute__((visibility ("default")))


#ifdef HAVE_ALIAS_ATTRIBUTE
#   define DLB_ALIAS(ret_type, name, decl_args, call_args, target) \
        ret_type name decl_args __attribute__((alias(#target)));
#else
#   define DLB_ALIAS(ret_type, name, decl_args, call_args, target) \
        ret_type name decl_args { return target call_args; }
#endif


#endif /* DLB_COMMON_H */
