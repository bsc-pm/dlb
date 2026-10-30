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

#include "talp/backends/perf_event_syscall.h"
#include <stdbool.h>
#include <stddef.h>

typedef struct fake_group_t {
    bool opened;
    int64_t counter;
} fake_group_t;

static __thread fake_group_t fake_group = {
    .opened = false,
    .counter = 0,
};

enum {
    FAKE_LEADER_FD = 100,
    FAKE_MEMBER_FD = 101,
};

int talp_perf_syscall_open_default(talp_perf_group_t *group) {

    if (fake_group.opened) return -1;

    fake_group.opened = true;
    fake_group.counter = 0;

    group->fds[0] = FAKE_LEADER_FD;
    group->fds[1] = FAKE_MEMBER_FD;

    return 0;
}

int talp_perf_syscall_enable(talp_perf_group_t *group) {

    if (group->fds[0] != FAKE_LEADER_FD || !fake_group.opened) return -1;

    fake_group.counter = 0;

    return 0;
}

int talp_perf_syscall_disable(talp_perf_group_t *group) {

    if (group->fds[0] != FAKE_LEADER_FD || !fake_group.opened) return -1;

    return 0;
}

hw_counters_t talp_perf_syscall_read(const talp_perf_group_t *group) {

    if (group->fds[0] != FAKE_LEADER_FD || !fake_group.opened) {
        return (hw_counters_t){ .cycles = -1, .instructions = -1 };
    }

    int64_t v = ++fake_group.counter;
    return (hw_counters_t){ .cycles = v, .instructions = v };
}

void talp_perf_syscall_close(talp_perf_group_t *group) {

    (void)group;

    fake_group.opened = false;
}
