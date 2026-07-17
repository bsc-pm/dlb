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

#ifndef ASSERT_PROCESS_H
#define ASSERT_PROCESS_H

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

void __gcov_dump() __attribute__((weak));

__attribute__((unused))
__attribute__((always_inline))
static inline void dlb_test__exit_process(int error)  {
    if (__gcov_dump) __gcov_dump();
    _exit(error);
}

__attribute__((unused))
__attribute__((always_inline))
static inline void wait_and_assert_process_with(pid_t pid, int error) {
    int wstatus;

    pid_t ret_pid = waitpid(pid, &wstatus, 0);
    assert(ret_pid > 0);
    assert(WIFEXITED(wstatus));
    assert(WEXITSTATUS(wstatus) == error);
}

__attribute__((unused))
__attribute__((always_inline))
static inline void wait_and_assert_process(pid_t pid) {
    wait_and_assert_process_with(pid, EXIT_SUCCESS);
}

#endif// ASSERT_PROCESS_H
