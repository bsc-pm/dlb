/*********************************************************************************/
/*  Copyright 2009-2021 Barcelona Supercomputing Center */
/*                                                                               */
/*  This file is part of the DLB library. */
/*                                                                               */
/*  DLB is free software: you can redistribute it and/or modify */
/*  it under the terms of the GNU Lesser General Public License as published by
 */
/*  the Free Software Foundation, either version 3 of the License, or */
/*  (at your option) any later version. */
/*                                                                               */
/*  DLB is distributed in the hope that it will be useful, */
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of */
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the */
/*  GNU Lesser General Public License for more details. */
/*                                                                               */
/*  You should have received a copy of the GNU Lesser General Public License */
/*  along with DLB.  If not, see <https://www.gnu.org/licenses/>. */
/*********************************************************************************/

/*<testinfo>
    test_generator="gens/basic-generator"
</testinfo>*/

#include "unique_shmem.h"

#include "LB_comm/shmem.h"
#include "LB_comm/shmem_mngo.h"
#include "apis/dlb_errors.h"
#include "support/mask_utils.h"

#include <assert.h>
#include <sched.h>
#include <sys/types.h>

static void assert_mngo_message_eq(const mngo_message_t *a,
                                   const mngo_message_t *b) {
  assert(a->mngo == b->mngo);
  assert(a->lewi == b->lewi);
  assert(a->drom == b->drom);
}

int main(int argc, char *argv[]) {

  enum { SYS_SIZE = 2 };

  mu_init();
  mu_testing_set_sys_size(SYS_SIZE);

  size_t manager_id_1;

  /* Initialize shmem mngo (first instance) */
  assert(shmem_mngo_init(SHMEM_KEY, &manager_id_1) == DLB_SUCCESS);

  /* Simulate a send to itself */
  {
    mngo_message_t message_read;
    mngo_message_t message_send = (const mngo_message_t){
        .mngo = MNGO_NONE,
        .lewi = ACTION_ENABLE_MODULE,
        .drom = ACTION_DISABLE_MODULE,
    };

    assert(shmem_mngo_send_message(manager_id_1, &message_send,
                                   DLB_MNGO_SHM_TRY) == DLB_SUCCESS);
    assert(shmem_mngo_read_message(manager_id_1, &message_read) == DLB_SUCCESS);

    assert_mngo_message_eq(&message_send, &message_read);
  }

  size_t manager_id_2;

  /* Initialize shmem mngo (first instance) */
  assert(shmem_mngo_init(SHMEM_KEY, &manager_id_2) == DLB_SUCCESS);

  /* Simulate a sent to another manager */
  {
    mngo_message_t message_read;
    mngo_message_t message_send = (const mngo_message_t){
        .mngo = MNGO_NONE,
        .lewi = ACTION_NONE,
        .drom = ACTION_ENABLE_MODULE,
    };

    assert(shmem_mngo_send_message(manager_id_2, &message_send,
                                   DLB_MNGO_SHM_TRY) == DLB_SUCCESS);
    assert(shmem_mngo_read_message(manager_id_2, &message_read) == DLB_SUCCESS);

    assert_mngo_message_eq(&message_send, &message_read);
  }

  /* Simulate broadcast */
  {
    mngo_message_t message_read_1, message_read_2;
    mngo_message_t message_send = (const mngo_message_t){
        .mngo = MNGO_NONE,
        .lewi = ACTION_NONE,
        .drom = ACTION_ENABLE_MODULE,
    };

    assert(shmem_mngo_broadcast_message(&message_send, DLB_MNGO_SHM_TRY) ==
           DLB_SUCCESS);

    assert(shmem_mngo_read_message(manager_id_1, &message_read_1) == DLB_SUCCESS);
    assert(shmem_mngo_read_message(manager_id_2, &message_read_2) == DLB_SUCCESS);

    assert_mngo_message_eq(&message_send, &message_read_1);
    assert_mngo_message_eq(&message_send, &message_read_2);
  }

  /* Finalize shmem mngo */
  assert(shmem_mngo_fini(manager_id_1) == DLB_SUCCESS);
  assert(shmem_mngo_fini(manager_id_2) == DLB_SUCCESS);
}
