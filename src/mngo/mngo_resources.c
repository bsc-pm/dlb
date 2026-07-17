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

#include "mngo/mngo_resources.h"
#include "LB_comm/shmem_mngo.h"
#include "LB_comm/shmem_procinfo.h"
#include "LB_core/DLB_kernel.h"
#include "LB_core/spd.h"
#include "dlb_errors.h"
#include "support/debug.h"
#include "support/mask_utils.h"

int mngo_state__load(subprocess_descriptor_t *spd, mngo_state_t *state) {
    // Make sure to stop lewi if is not available for this region
    // (!lewi_enabled), and always enable it when its available (lewi_enabled)
    // for that region and MNGO has decided that is useful for this region
    // (lewi_on).
    set_lewi_enabled(spd, spd->options.lewi && state->lewi_on);

    if (spd->options.drom) {
        int error;

        cpu_set_t current_mask;
        shmem_procinfo__getprocessmask(0, &current_mask, DLB_DROM_FLAGS_NONE);
        verbose(VB_MNGO, "(Loading State) Old mask: %s",
                mu_to_str(&current_mask));

        // For DROM to update the process mask correctly, we need to first free
        // the cpus that other processes might add in their mask. For that we
        // need to set our mask to the cpus that we have and are going to keep,
        // this operation is the CPU_AND.
        cpu_set_t intermediate_mask;
        CPU_AND(&intermediate_mask, &current_mask, &state->drom_mask);

        verbose(VB_MNGO, "(Loading State) Int mask: %s",
                mu_to_str(&intermediate_mask));
        error = drom_setprocessmask(0, &intermediate_mask, DLB_DROM_FLAGS_NONE);
        if (error != DLB_SUCCESS) {
            verbose(
                VB_MNGO,
                "Could not free cpus by setting process intermediate mask %s",
                mu_to_str(&intermediate_mask));
            return DLB_NOUPDT;
        }

        // We then syncronize with all the other processes from the node to
        // make sure that when we set the new_mask all the cpus that we'll
        // aquire will be in the free list.
        shmem_mngo_barrier_wait();

        verbose(VB_MNGO, "(Loading State) New mask: %s",
                mu_to_str(&(state->drom_mask)));
        error = drom_setprocessmask(0, &(state->drom_mask), DLB_DROM_FLAGS_NONE);
        if (error != DLB_SUCCESS) {
            verbose(VB_MNGO, "Could not register new mask %s",
                    mu_to_str(&state->drom_mask));
            return DLB_ERR_PERM;
        }
    }

    return DLB_SUCCESS;
}
