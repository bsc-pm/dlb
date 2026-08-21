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

#include "LB_core/fork_handler.h"

#include "LB_comm/shmem_async.h"
#include "LB_comm/shmem_barrier.h"
#include "LB_comm/shmem_cpuinfo.h"
#include "LB_comm/shmem_lewi_async.h"
#include "LB_comm/shmem_lewi_light.h"
#include "LB_comm/shmem_mngo.h"
#include "LB_comm/shmem_procinfo.h"
#include "LB_comm/shmem_talp.h"
#include "LB_core/node_barrier.h"
#include "LB_core/spd.h"
#include "LB_core/thread_ctx.h"
#include "support/tracing.h"

#ifdef MPI_LIB
#include "mpi/mpi_core.h"
#endif

#include <pthread.h>

static void dlb_atfork_prepare(void) {
    shmem_async__atfork_prepare();
    shmem_cpuinfo__atfork_prepare();
    shmem_lewi_async__atfork_prepare();
    shmem_lewi_light__atfork_prepare();
    shmem_mngo__atfork_prepare();
    shmem_procinfo__atfork_prepare();
    shmem_talp__atfork_prepare();
    node_barrier__atfork_prepare();
    spd_atfork_prepare();
}

static void dlb_atfork_parent(void) {
    shmem_async__atfork_parent();
    shmem_cpuinfo__atfork_parent();
    shmem_lewi_async__atfork_parent();
    shmem_lewi_light__atfork_parent();
    shmem_mngo__atfork_parent();
    shmem_procinfo__atfork_parent();
    shmem_talp__atfork_parent();
    node_barrier__atfork_parent();
    spd_atfork_parent();
}

static void dlb_atfork_child(void) {
    shmem_async__atfork_child();
    shmem_cpuinfo__atfork_child();
    shmem_lewi_async__atfork_child();
    shmem_lewi_light__atfork_child();
    shmem_mngo__atfork_child();
    shmem_procinfo__atfork_child();
    shmem_talp__atfork_child();
    node_barrier__atfork_child();
    spd_atfork_child();

    shmem_barrier__reset_after_fork();
    instrument_reset_after_fork();
    thread_ctx_reset_after_fork();
#if MPI_LIB
    mpi_core_reset_after_fork();
#endif
}

static pthread_once_t once = PTHREAD_ONCE_INIT;

static void do_register(void) {
    pthread_atfork(dlb_atfork_prepare, dlb_atfork_parent, dlb_atfork_child);
}

void fork_handler_init(void) {
    pthread_once(&once, do_register);
}
