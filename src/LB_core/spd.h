/*********************************************************************************/
/*  Copyright 2017 Barcelona Supercomputing Center                               */
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

#ifndef SPD_H
#define SPD_H

#include "LB_core/lb_funcs.h"
#include "LB_numThreads/numThreads.h"
#include "support/options.h"
#include "support/types.h"

#include <sys/types.h>

/* Sub-process Descriptor */

typedef struct SubProcessDescriptor {
    pid_t id;
    int *cpus_priority_array;
    cpu_set_t process_mask;
    cpu_set_t active_mask;
    options_t options;
    pm_interface_t pm;
    policy_t lb_policy;
    balance_policy_t lb_funcs;
} subprocess_descriptor_t;

#endif /* SPD_H */
