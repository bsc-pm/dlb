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

#ifndef BALANCER_H
#define BALANCER_H

#include <stdbool.h>
#include <stdlib.h>

int mngo_balancer(size_t id, int *deltas, size_t size);

void mngo_balancer_testing__pack_levels(size_t my_abs_delta, int * restrict levels, int * restrict deltas, size_t level);

int mngo_balancer_testing__precompute_levels(int * restrict deltas, size_t deltas_size, int * restrict count_add, int * restrict count_sub, size_t count_size);

int mngo_balancer_testing__coherent_redistribution(bool i_am_a_giver, size_t my_abs_delta, int * restrict count_add, int * restrict count_sub, size_t count_size);

#endif //BALANCER_H
