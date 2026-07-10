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

#ifndef SAMPLE_H
#define SAMPLE_H

#include "talp/talp_types.h"

void talp_sample_init(talp_info_t *talp_info);
void talp_sample_finalize(talp_info_t *talp_info);

bool talp_sample_is_main_sequential(void);
void talp_sample_set_main_sequential(bool is_sequential);
talp_sample_t* talp_sample_get(talp_info_t *talp_info);

void talp_sample_update(talp_info_t *talp_info);
void talp_sample_set_state(talp_info_t *talp_info, talp_sample_state_t new_state);

void talp_sample_aggregate_all_to_macrosample(
        talp_info_t *restrict talp_info, talp_macrosample_t *restrict macrosample);

#endif /* SAMPLE_H */
