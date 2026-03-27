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

#ifndef TEST_PROCESS_H
#define TEST_PROCESS_H

#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

void __gcov_flush(void) __attribute__((weak));
void __gcov_dump(void) __attribute__((weak));

static inline void dlb_test__gcov_dump(void) {
    if      (__gcov_flush) __gcov_flush();
    else if (__gcov_dump)  __gcov_dump();
}

static inline void dlb_test__exit(int error) {
    dlb_test__gcov_dump();
    _exit(error);
}

#endif /* TEST_PROCESS_H */
