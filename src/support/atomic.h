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

#ifndef ATOMIC_H
#define ATOMIC_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#if defined(HAVE_STDATOMIC_H) && !defined(__INTEL_COMPILER)
#include <stdatomic.h>

#define DLB_ATOMIC_ADD(ptr, val)            atomic_fetch_add(ptr, val)
#define DLB_ATOMIC_ADD_RLX(ptr, val)        atomic_fetch_add_explicit(ptr, val, memory_order_relaxed)
#define DLB_ATOMIC_ADD_FETCH(ptr, val)      atomic_fetch_add(ptr, val) + val
#define DLB_ATOMIC_ADD_FETCH_RLX(ptr, val)  atomic_fetch_add_explicit(ptr, val, memory_order_relaxed) + val
#define DLB_ATOMIC_SUB(ptr, val)            atomic_fetch_sub(ptr, val)
#define DLB_ATOMIC_SUB_RLX(ptr, val)        atomic_fetch_sub_explicit(ptr, val, memory_order_relaxed)
#define DLB_ATOMIC_SUB_FETCH(ptr, val)      atomic_fetch_sub(ptr, val) - val
#define DLB_ATOMIC_SUB_FETCH_RLX(ptr, val)  atomic_fetch_sub_explicit(ptr, val, memory_order_relaxed) - val
#define DLB_ATOMIC_LD(ptr)                  atomic_load(ptr)
#define DLB_ATOMIC_LD_RLX(ptr)              atomic_load_explicit(ptr, memory_order_relaxed)
#define DLB_ATOMIC_LD_ACQ(ptr)              atomic_load_explicit(ptr, memory_order_acquire)
#define DLB_ATOMIC_ST(ptr, val)             atomic_store(ptr, val)
#define DLB_ATOMIC_ST_RLX(ptr, val)         atomic_store_explicit(ptr, val, memory_order_relaxed)
#define DLB_ATOMIC_ST_REL(ptr, val)         atomic_store_explicit(ptr, val, memory_order_release)

#else /* not HAVE_STDATOMIC_H */

#define atomic_int              int
#define atomic_uint             unsigned int
#define atomic_int_least64_t    int64_t

#define DLB_ATOMIC_ADD(ptr, val)            __sync_fetch_and_add(ptr, val)
#define DLB_ATOMIC_ADD_RLX(ptr, val)        __sync_fetch_and_add(ptr, val)
#define DLB_ATOMIC_ADD_FETCH(ptr, val)      __sync_add_and_fetch(ptr, val)
#define DLB_ATOMIC_ADD_FETCH_RLX(ptr, val)  __sync_add_and_fetch(ptr, val)
#define DLB_ATOMIC_SUB(ptr, val)            __sync_fetch_and_sub(ptr, val)
#define DLB_ATOMIC_SUB_RLX(ptr, val)        __sync_fetch_and_sub(ptr, val)
#define DLB_ATOMIC_SUB_FETCH(ptr, val)      __sync_sub_and_fetch(ptr, val)
#define DLB_ATOMIC_SUB_FETCH_RLX(ptr, val)  __sync_sub_and_fetch(ptr, val)
#define DLB_ATOMIC_LD(ptr)                  \
    ({ typeof (*ptr) value; __sync_synchronize(); value = (*ptr); __sync_synchronize(); value; })
#define DLB_ATOMIC_LD_RLX(ptr)              (*ptr)
#define DLB_ATOMIC_LD_ACQ(ptr)              ({ __sync_synchronize(); (*ptr); })
#define DLB_ATOMIC_ST(ptr, val)             __sync_synchronize(); (*ptr) = (val); __sync_synchronize()
#define DLB_ATOMIC_ST_RLX(ptr, val)         (*ptr) = (val)
#define DLB_ATOMIC_ST_REL(ptr, val)         (*ptr) = (val); __sync_synchronize()

#endif
#endif /* ATOMIC_H */
