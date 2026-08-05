/*********************************************************************************/
/*  Copyright 2009-2026 Barcelona Supercomputing Center                          */
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

#ifndef GPU_MASK_UTILS_H
#define GPU_MASK_UTILS_H

#include <stdint.h>

/* More than enough in any real case,
 * predefined to match local uint64_t bitset and ease MPI comms. */
enum { MAX_NODE_GPUS = 64 };

static inline int gm_count(uint64_t x) {
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_popcountll(x);
#else
    /* http://en.wikipedia.org/wiki/Hamming_weight
     * (Good approach for few nonzero bits) */
    int count;
    for (count=0; x; count++)
        x &= x - 1;
    return count;
#endif
}

/* Returns the number of trailing 0-bits in x, starting at the least significant bit position. */
static inline int gm_ctz(uint64_t x)
{
#if defined(__GNUC__) || defined(__clang__)
    return x ? __builtin_ctzll(x) : -1;
#else
    if (x == 0)
        return -1;

    int n = 0;
    while ((x & 1) == 0) {
        x >>= 1;
        ++n;
    }
    return n;
#endif
}

/* Clear least significat bit */
static inline uint64_t gm_clear_lsb(uint64_t x)
{
    return x & (x - 1);
}

/* Check if bit is set */
static inline int gm_isset(uint32_t bit, uint64_t x) {
    return (x & (1ULL << bit)) ? 1 : 0;
}

#endif /* GPU_MASK_UTILS_H */
