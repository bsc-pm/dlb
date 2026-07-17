/*********************************************************************************/
/*  Copyright 2009-2023 Barcelona Supercomputing Center                          */
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

int main(int argc, char *argv[]) {

  enum { SYS_SIZE = 2 };

  mu_init();
  mu_testing_set_sys_size(SYS_SIZE);

  pid_t pid1 = 1;
  size_t manager_id_1;

  /* Initialize shmem mngo (first instance) */
  assert(shmem_mngo_init(SHMEM_KEY, pid1, 0, &manager_id_1) == DLB_SUCCESS);

  /* Simulate inbox is full (read) */
  {
  }

  pid_t pid2 = 2;
  size_t manager_id_2;
  assert(shmem_mngo_init(SHMEM_KEY, pid2, 1, &manager_id_2) == DLB_SUCCESS);
  /* Simulate inbox is full (broadcast) */
  {
  }

  /* Simulate inbox is empty */
  {
  }

  /* Simulate perform operation with mid bigger than max */
  {
  }

  /* Initialize shmem when all slots are allocated */
  {
    pid_t pid3 = 3;
    size_t manager_id_3;
    assert(DLB_ERR_NOSHMEM ==
     shmem_mngo_init(SHMEM_KEY, pid3, 3, &manager_id_3));
  }

  /* Finalize shmem mngo */
  assert(shmem_mngo_fini(manager_id_1) == DLB_SUCCESS);
  assert(shmem_mngo_fini(manager_id_2) == DLB_SUCCESS);
}
