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

#include <stdbool.h>

/* Atomic operations */

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
#define DLB_ATOMIC_EXCH(ptr, val)           atomic_exchange(ptr, val)
#define DLB_ATOMIC_EXCH_RLX(ptr, val)       atomic_exchange_explicit(ptr, val, memory_order_relaxed)
#define DLB_ATOMIC_CMP_EXCH_WEAK(ptr, expected, desired) \
                                            atomic_compare_exchange_weak(ptr, &expected, desired)

#else /* not HAVE_STDATOMIC_H */

#define _Atomic(T)              volatile __typeof__(T)
#define atomic_int              volatile int
#define atomic_uint             volatile unsigned int
#define atomic_int_least64_t    volatile int64_t
#define atomic_uint_least64_t   volatile uint64_t
#define atomic_bool             volatile bool

#define DLB_ATOMIC_ADD(ptr, val)            __sync_fetch_and_add(ptr, val)
#define DLB_ATOMIC_ADD_RLX(ptr, val)        DLB_ATOMIC_ADD(ptr, val)
#define DLB_ATOMIC_ADD_FETCH(ptr, val)      __sync_add_and_fetch(ptr, val)
#define DLB_ATOMIC_ADD_FETCH_RLX(ptr, val)  DLB_ATOMIC_ADD_FETCH(ptr, val)
#define DLB_ATOMIC_SUB(ptr, val)            __sync_fetch_and_sub(ptr, val)
#define DLB_ATOMIC_SUB_RLX(ptr, val)        DLB_ATOMIC_SUB(ptr, val)
#define DLB_ATOMIC_SUB_FETCH(ptr, val)      __sync_sub_and_fetch(ptr, val)
#define DLB_ATOMIC_SUB_FETCH_RLX(ptr, val)  __sync_sub_and_fetch(ptr, val)
#define DLB_ATOMIC_LD(ptr)                  \
    ({ typeof (*ptr) value; __sync_synchronize(); value = (*ptr); __sync_synchronize(); value; })
#define DLB_ATOMIC_LD_RLX(ptr)              (*ptr)
#define DLB_ATOMIC_LD_ACQ(ptr)              ({ __sync_synchronize(); (*ptr); })
#define DLB_ATOMIC_ST(ptr, val)             __sync_synchronize(); (*ptr) = (val); __sync_synchronize()
#define DLB_ATOMIC_ST_RLX(ptr, val)         (*ptr) = (val)
#define DLB_ATOMIC_ST_REL(ptr, val)         (*ptr) = (val); __sync_synchronize()
#define DLB_ATOMIC_EXCH(ptr, val)           __sync_synchronize(); __sync_lock_test_and_set(ptr, val)
#define DLB_ATOMIC_EXCH_RLX(ptr, val)       __sync_lock_test_and_set(ptr, val)
#define DLB_ATOMIC_CMP_EXCH_WEAK(ptr, oldval, newval) \
                                            __sync_bool_compare_and_swap(ptr, oldval, newval)

#endif


/* Support for cache alignment, padding, etc. */

#ifndef CACHE_LINE
#define CACHE_LINE 128
#endif

#define DLB_ALIGN_CACHE __attribute__((aligned(CACHE_LINE)))


/* If flags does not contain 'bit', atomically:
 *  - set 'bit'
 *  - return true
 * Otherwise:
 *  - return false
 */
static inline bool set_bit(atomic_int *flags, int bit) {
    if (!bit) return false;
    int oldval = *flags;
    int newval;
    do {
        if (oldval & bit) {
            /* flag already contains bit */
            return false;
        }
        newval = oldval | bit;
    } while (!DLB_ATOMIC_CMP_EXCH_WEAK(flags, oldval, newval));
    return true;
}

/* If flags does not contain 'bit', atomically:
 *  - set 'bit'
 *  - return true
 * Otherwise:
 *  - return false
 */
static inline bool clear_bit(atomic_int *flags, int bit) {
    if (!bit) return false;
    int oldval = *flags;
    int newval;
    do {
        if (!(oldval & bit)) {
            /* flag does not contain bit */
            return false;
        }
        newval = oldval & ~bit;
    } while (!DLB_ATOMIC_CMP_EXCH_WEAK(flags, oldval, newval));
    return true;
}

/* If flags contains 'expected', atomically:
 *  - clear 'expected'
 *  - set 'desired'
 *  - return true
 * Otherwise:
 *  - return false
 */
static inline bool cas_bit(atomic_int *flags, int expected, int desired) {
    int oldval = *flags;
    int newval;
    do {
        if (!(oldval & expected)
                && !(oldval == 0 && expected == 0)) {
            /* flag does not contain expected */
            return false;
        }
        newval = oldval;
        newval &= ~expected;
        newval |= desired;
    } while (!DLB_ATOMIC_CMP_EXCH_WEAK(flags, oldval, newval));
    return true;
}

/* If flags does not contain 'set', atomically:
 *  - set 'set'
 *  - clear 'clear'
 *  - return true
 * Otherwise:
 *  - return false
 */
static inline bool test_set_clear_bit(atomic_int *flags, int set, int clear) {
    int oldval = *flags;
    int newval;
    do {
        if (oldval & set
                || (oldval == 0 && set == 0)) {
            /* flag is already set */
            return false;
        }
        newval = oldval;
        newval &= ~clear;
        newval |= set;
    } while (!DLB_ATOMIC_CMP_EXCH_WEAK(flags, oldval, newval));
    return true;
}


#endif /* ATOMIC_H */
