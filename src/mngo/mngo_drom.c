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

#include "mngo/mngo_drom.h"

#include "LB_comm/shmem_procinfo.h"
#include "dlb_errors.h"
#include "mngo/mngo.h"
#include "mngo/mngo_balancer.h"

#include "LB_comm/shmem_mngo.h"
#include "support/mask_utils.h"
#include <math.h>

/*
 * To decide how many cores DROM has to change we will solve for the number of
 * processors that have to be removed from one process with *self* parallel
 * efficiency to achieve *others* parallel efficiency, being this lower or
 * greater than *self*.
 */
static int balance(const subprocess_descriptor_t *spd, float self,
                   float others) {

    mngo_info_t *mngo_info = spd->mngo_info;
    cpu_set_t mask;
    shmem_procinfo__getprocessmask(shmem_mngo_get_pid(mngo_info->mid), &mask,
                                   DLB_DROM_FLAGS_NONE);

    /*
     * TODO: Implement also the cross-correlation of shared memory parallel
     * efficiencies among different processes, to determine if reducing the
     * number of active cores in the node (or activating EAR, in the future)
     * will be useful or not.
     *
     * For now will activate DROM and reduce the amount of cores of the affected
     * node with the hope that the load imbalance is persistent among nodes and
     * DROM will be useful. And roll the decision back if performance in any
     * node gets worse on the next observation.
     */

    /*
     *                 +--------------------------+---------------+
     * (ex. 1) SELF PE | GOOD                     | BAD           |
     *                 +--------------------------+---------------+
     *                 +------------------------------------+-----+
     * (ex. 2) SELF PE | GOOD                               | BAD |
     *                 +------------------------------------+-----+
     *                 +--------------------------------+---------+
     *    AVG. NODE PE | GOOD                           | BAD     |
     *                 +--------------------------------+---------+
     *
     * With the ratio operation we convert it to that:
     *
     *                 +--------------------------+-----+
     * (ex. 1) SELF PE | GOOD                     | BAD |
     *                 +--------------------------+-----+
     *                 +--------------------------------+---+
     * (ex. 2) SELF PE | GOOD                           | E |
     *                 +--------------------------------+---+
     */
    float ratio_mpi_parallel_efficiency = ((float)self) / others;

    /*
     * PE = (GOOD + EXTRA) / (GOOD + BAD)
     *
     * To reduce the load balance we need make the elapsed time of the GOOD part
     * take the same amount of time for all processes.
     *
     * Assuming that the aggregated CPU time is constant no matter how many
     * cores a process has (i.e. ideal thread scalability). That is, the area
     * described by the GOOD elapsed time and the number of cores (N) is
     * constant for all values of N.
     *
     *                                                  +----+
     *                                                  |    |
     *        +----------+                              |    |
     *  N = 2 |          |  has the same area as  N = 4 |    |
     *        +----------+                              |    |
     *         GOOD = 12                                +----+
     *                                                 GOOD = 6
     *
     * This is obviously an hover-optimistic assumption and it will hold true
     * depending on how good the application scales in OpenMP. But will
     * generally hold true for scaling down the number of processes.
     *
     * TODO: When we have OpenMP metrics we can scale the number of processes to
     * change taking into account the OpenMP parallel efficiency.
     *
     * This area is described by the product of the number of processes and the
     * GOOD part. And we want to change the GOOD elapsed time to force the value
     * to take the same as the average GOOD elapsed. From this objective we
     * obtain the following equation:
     *
     *  N * ( GOOD + DIFFERENCE ) = (N + dN) * GOOD
     *
     *  Where dN is the change in number of cores. When dN is isolated we
     * obtain:
     *
     *  N * GOOD + dN * GOOD = N * ( GOOD + DIFFERENCE )
     *
     *  dN * GOOD = N * ( GOOD + DIFFERENCE ) - N * GOOD
     *
     *           ( GOOD + DIFFERENCE )
     *  dN = N * --------------------- - N = N * PE - N
     *                   GOOD
     *
     *  Due to that these operations are continuous but the dN has to be a
     *  discrete value. We ceil the multiplication to not release a little bit
     *  more cores than needed.
     */
    int core_count = mu_count_cores_intersecting_with_cpuset(&mask);
    int core_change =
        (int)ceil((float)core_count * ratio_mpi_parallel_efficiency) -
        core_count;

    /*
     * Keep at least one core.
     */
    int maximum_core_release = -core_count + 1;
    int bounded_core_change = max_int(core_change, maximum_core_release);

    return bounded_core_change;
}

static void print_redistribution_report(const subprocess_descriptor_t *spd,
                                        int self_cpu_delta) {
    mngo_info_t *mngo_info = spd->mngo_info;

    const char *region_name = "";
    mngo_regions_manager_t *region_manager = &mngo_info->region_handler;
    dlb_mngo_region_t **current_region;
    if (queue__peek_head(region_manager->active_regions,
                         (void **)&current_region) == DLB_SUCCESS) {
        region_name = (*current_region)->name;
    }
    shmem_mngo_drom__print_redistribution(mngo_info->mid, self_cpu_delta,
                                          region_name);
}

/*
 * Once each process has used its history of performance to decide what actions
 * to take, we put all the actions from all processes together to decide which
 * actions have preference over which other actions.
 *
 * To do that we will use MPI to execute an all to all communication and then
 * apply the same logic to all the collected data to leave all the mngo
 * decisions in a consistant state.
 */
void mngo_drom__balance(const subprocess_descriptor_t *spd, float self_pe,
                        float node_pe, cpu_set_t *new_cpu_mask) {

    mngo_info_t *mngo_info = spd->mngo_info;

    // 1. Decide how many CPU i would like to give/take
    int self_cpu_delta = balance(spd, self_pe, node_pe);

    // 2. Communicate these to other processes
    int num_cpu_deltas =
        shmem_mngo_drom__alltoall_deltas_start(mngo_info->mid, self_cpu_delta);

    int *cpu_deltas = calloc(num_cpu_deltas, sizeof(int));
    shmem_mngo_drom__alltoall_deltas_finish(mngo_info->mid, cpu_deltas);

    // 3. Update how many CPU to give/take based on availability
    int coherent_self_cpu_delta =
        mngo_balancer(mngo_info->mid, cpu_deltas, num_cpu_deltas);
    free(cpu_deltas);

    // 4. Exchange CPUs
    shmem_mngo_drom__redistribute(mngo_info->mid, coherent_self_cpu_delta,
                                  new_cpu_mask);

    // 5 Show CPU changes
    print_redistribution_report(spd, coherent_self_cpu_delta);
}
