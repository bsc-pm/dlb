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

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#include "mngo/mngo_balancer.h"

/**
 * This file's task is to find a coherent (among all participants) ammount of
 * cores that a process can change. For example: if one process requests 2
 * cores, another 3 cores, but only one releases 4 cores. We must find a
 * distribution makes the total ammount cores given equal to the total ammount
 * received.
 */

static size_t precompute_levels(int *restrict deltas, size_t deltas_size,
                                int *restrict count_add,
                                int *restrict count_sub, size_t count_size);

static int coherent_redistribution(bool i_am_a_giver, size_t my_abs_delta,
                                   int *restrict count_add,
                                   int *restrict count_sub, size_t count_size);

int mngo_balancer(size_t id, int *deltas, size_t size) {

    size_t max = 0;
    for (size_t i = 0; i < size; i++) {
        size_t abs_delta = (size_t)abs(deltas[i]);
        if (max < abs_delta) max = abs_delta;
    }

    // We allocate up to max possition, because count_add[max] and
    // count_sub[max] holds how many processes want to take or give
    // (respectively) up-to max ammount of CPUs.
    int *count_add = calloc(max + 1, sizeof(int));
    int *count_sub = calloc(max + 1, sizeof(int));

    // precompute_levels if the maximum among deltas is smaller than max this
    // will provide the new max value. To avoid not needed computation during
    // the next phase.
    max = precompute_levels(deltas, size, count_add, count_sub, max);

    size_t self_abs_delta = abs(deltas[id]);
    bool i_am_a_giver = deltas[id] < 0;
    int coherent_delta = coherent_redistribution(i_am_a_giver, self_abs_delta,
                                                 count_add, count_sub, max);

    free(count_add);
    free(count_sub);

    return coherent_delta;
}

/*! \brief From a list of integer differences, compute how many give more than a
 * value.
 *  \param[in] deltas An integer array of size deltas_size, with differences
 * from different items.
 *  \param[in] deltas_size Size fo the deltas array.
 *  \param[out] count_add Array where count_add[N] shows how many items have
 * positive N or bigger.
 *  \param[out] count_sub Array where count_sub[N] shows how many items have
 * negative N or lower.
 *  \param[in] count_size Maximum size of count_add and count_sub.
 *
 *  This function counts how many items in deltas have values of N or bigger for
 * all N in {0, max(deltas)}.
 */
static size_t precompute_levels(int *restrict deltas, size_t deltas_size,
                                int *restrict count_add,
                                int *restrict count_sub, size_t count_size) {

    size_t final_count_size = 0;

    for (size_t id = 0; id < deltas_size; id++) {
        // We have to continue when `deltas[id] == 0` to maintain the position
        // `0` of the arrays the same as position `1`. To maintain the maximum
        // value in the arrays properly.
        if (deltas[id] == 0) continue;

        size_t abs_delta = abs(deltas[id]);

        // Saturate the value of abs_delta, to avoid going out of bounds of the
        // arrays.
        size_t sat_abs_delta = abs_delta < count_size ? abs_delta : count_size;

        // Update the highest value
        if (sat_abs_delta > final_count_size) {
            final_count_size = sat_abs_delta;
        }

        // Update the levels
        if (deltas[id] > 0) {
            for (size_t l = 0; l <= sat_abs_delta; l++) {
                count_add[l]++;
            }
        } else {
            for (size_t l = 0; l <= sat_abs_delta; l++) {
                count_sub[l]++;
            }
        }
    }

    return final_count_size;
}

static void pack_levels(size_t my_abs_delta, int *restrict levels,
                        int *restrict deltas, size_t id);

/**
 * This function aims to redistribute CPUs across processes coherently after
 * each process has communicated how many processes would like to give/take,
 * without more communication.
 *
 * To achieve that it uses the aggregated data of how many processes want to
 * give/take up-to a level of CPUs in the arrays `count_add` and `count_sub`.
 *
 * What is a level in this context:
 *  - A level is the number of CPUs a process wants to give or take.
 *
 * Local parameters (to decide what this process needs to do):
 *  - i_am_a_giver: true when this process wants to reduce the number of cpus
 *  - my_abs_delta: the positive value of the number of CPUs this process wants
 * to give or get
 *
 * Remote parameters (to know what the others will do):
 *  - count_add: an array holding how many processes take CPUs for each level
 *  - count_sub: an array holding how many processes give CPUs for each level
 *  - max_level: the heighest valid level. group_deltas, count_add, count_sub
 * should have allocated and valid data up to the max_level position.
 */
static int coherent_redistribution(bool i_am_a_giver, size_t my_abs_delta,
                                   int *restrict count_add,
                                   int *restrict count_sub, size_t count_size) {

    int my_coherent_delta = 0;

    int *group_deltas = calloc(count_size + 1, sizeof(int));
    for (size_t i = 0; i <= count_size && i <= my_abs_delta; i++) {
        group_deltas[i] = 1;
    }

    for (size_t l_add = count_size; l_add >= 1; l_add--) {
        if (count_add[l_add] == 0) continue; // Empty level

        size_t l_sub;
        bool match_found = false;
        for (l_sub = count_size; l_sub >= 1; l_sub--) {
            if (count_sub[l_sub] == 0) continue; // Empty level

            // Pack with lower level if no match will be found
            if (count_add[0] < count_sub[l_sub]) {
                pack_levels(my_abs_delta, count_add, group_deltas, l_add);
                break;
            }
            if (count_add[l_add] > count_sub[0]) {
                pack_levels(my_abs_delta, count_sub, group_deltas, l_sub);
                continue;
            }

            // We find a match
            if (count_add[l_add] == count_sub[l_sub]) {
                match_found = true;
                count_add[l_add] = 0;
                count_sub[l_sub] = 0;
                break;
            }
        }

        if (match_found) {
            if (i_am_a_giver) {
                if (l_sub <= my_abs_delta) {
                    my_coherent_delta -= group_deltas[l_sub];
                    group_deltas[l_sub] = 0;
                }
            } else {
                if (l_add <= my_abs_delta) {
                    my_coherent_delta += group_deltas[l_add];
                    group_deltas[l_add] = 0;
                }
            }
        }
    }

    free(group_deltas);

    return my_coherent_delta;
}

static void pack_levels(size_t my_abs_delta, int *restrict levels,
                        int *restrict deltas, size_t level) {

    /* Pack the level */
    levels[level - 1] += levels[level];
    levels[level] = 0;

    /* If our level is within range we pack the deltas too. */
    if (level <= my_abs_delta) {
        deltas[level - 1] += deltas[level];
        deltas[level] = 0;
    }

    /* Keep position 0 (i.e. the maximum) updated. */
    if (levels[0] < levels[level - 1]) {
        levels[0] = levels[level - 1];
    }
}

/*********************************************************************************/
/*    Functions for testing purposes */
/*********************************************************************************/

void mngo_balancer_testing__pack_levels(size_t my_abs_delta,
                                        int *restrict levels,
                                        int *restrict deltas, size_t level) {
    return pack_levels(my_abs_delta, levels, deltas, level);
}

int mngo_balancer_testing__precompute_levels(int *restrict deltas,
                                             size_t deltas_size,
                                             int *restrict count_add,
                                             int *restrict count_sub,
                                             size_t count_size) {
    return precompute_levels(deltas, deltas_size, count_add, count_sub,
                             count_size);
}

int mngo_balancer_testing__coherent_redistribution(bool i_am_a_giver,
                                                   size_t my_abs_delta,
                                                   int *restrict count_add,
                                                   int *restrict count_sub,
                                                   size_t count_size) {
    return coherent_redistribution(i_am_a_giver, my_abs_delta, count_add,
                                   count_sub, count_size);
}
