/*********************************************************************************/
/*  Copyright 2009-2025 Barcelona Supercomputing Center                          */
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

#ifndef TALP_PERF_SYSCALL_H
#define TALP_PERF_SYSCALL_H

#include "talp/backend.h"  /* hw_counters_t */

/* Thin wrapper layer around the raw perf_event_open/ioctl/read calls,
so that the plugin logic in perf.c can be tested against a fake
implementation instead of requiring real hardware/kernel access for the tests */


typedef struct talp_perf_group_t {
    // first fd is cycles
    // second fd is instructions
    int fds[2];
} talp_perf_group_t;

// wrappers for the actual syscalls.
// -1 means failure, while 0 is success

// wraps perf_event_open
int talp_perf_syscall_open_default(talp_perf_group_t *group);

// wraps ioctl( PERF_EVENT_IOC_RESET, PERF_IOC_FLAG_GROUP) & ioctl( PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP);
int talp_perf_syscall_enable(talp_perf_group_t *group);

// wraps ioctl( PERF_EVENT_IOC_DISABLE, PERF_IOC_FLAG_GROUP)
int talp_perf_syscall_disable(talp_perf_group_t *group);

// wraps the read(). returns -1 inside the hwc if something went wrong
hw_counters_t talp_perf_syscall_read(const talp_perf_group_t *group);

//  Closes every fd in the group 
void talp_perf_syscall_close(talp_perf_group_t *group);

#endif /* TALP_PERF_SYSCALL_H */
