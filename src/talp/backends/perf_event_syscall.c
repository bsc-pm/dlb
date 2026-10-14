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

#include <stdint.h>
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "talp/backends/perf_event_syscall.h"
#include "talp/backends/backend_utils.h"

#include <linux/perf_event.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

int talp_perf_syscall_open_default(talp_perf_group_t *group) {
    
    // perf organizes the counter values into groups. Each group has a leader and other members.
    // So we first create the leader as Cycles and add instructions as member

    struct perf_event_attr leader_attr = {
        .type = PERF_TYPE_HARDWARE,
        .size = sizeof(struct perf_event_attr),
        .config = PERF_COUNT_HW_CPU_CYCLES,
        .disabled = 1, // do not start the group right away
        .exclude_kernel = 1, // we are not interested in kernel instructions?
        .exclude_hv = 1,
        .read_format = PERF_FORMAT_GROUP,
    };
    
    pid_t pid = 0; // this process
    int cpu = -1; // any CPU
    int initial_group_fd = -1; // as we are creating the leader


    int leader_fd = (int)syscall(SYS_perf_event_open, &leader_attr,pid,cpu, initial_group_fd,
            PERF_FLAG_FD_CLOEXEC);

    if (leader_fd < 0) {
        PLUGIN_WARNING("perf_event_open (cycles) failed: %s\n", strerror(errno));
        return -1;
    }


    // lets just copy and modify what we need
    struct perf_event_attr member_attr = leader_attr;
    member_attr.config = PERF_COUNT_HW_INSTRUCTIONS;
    member_attr.disabled = 0; // enable it, as it will not run directly as the leader is still disabled.

    int member_fd = (int)syscall(SYS_perf_event_open, &member_attr, pid, cpu, leader_fd,
            PERF_FLAG_FD_CLOEXEC);
    if (member_fd < 0) {
        PLUGIN_WARNING("perf_event_open (instructions) failed: %s\n", strerror(errno));
        close(leader_fd);
        return -1;
    }

    group->fds[0] = leader_fd;
    group->fds[1] = member_fd;
    return 0;
}

int talp_perf_syscall_enable(talp_perf_group_t *group) {

    int leader_fd = group->fds[0];
    
    int reset = ioctl(leader_fd, PERF_EVENT_IOC_RESET, PERF_IOC_FLAG_GROUP);
    if(reset != 0){
        PLUGIN_WARNING("perf_event: failed to reset counters: %s\n",
                strerror(errno));
        return -1;
    }

    int enable= ioctl(leader_fd, PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP);
    if(enable != 0){
        PLUGIN_WARNING("perf_event: failed to enable counters: %s\n",
                strerror(errno));
        return -1;
    }

    return 0;
}

int talp_perf_syscall_disable(talp_perf_group_t *group) {

    if (ioctl(group->fds[0], PERF_EVENT_IOC_DISABLE, PERF_IOC_FLAG_GROUP) != 0) {
        PLUGIN_WARNING("perf_event: failed to disable counters: %s\n", strerror(errno));
        return -1;
    }

    return 0;
}

hw_counters_t talp_perf_syscall_read(const talp_perf_group_t *group) {

    struct {
        uint64_t nr;
        uint64_t values[2];
    } buf;

    ssize_t bytes_read = read(group->fds[0], &buf, sizeof(buf));
    if (bytes_read != (ssize_t)sizeof(buf) || buf.nr != 2) {
        PLUGIN_WARNING("perf_event: failed to read counters: %s\n", strerror(errno));
        return (hw_counters_t){ .cycles = -1, .instructions = -1 };
    }

    return (hw_counters_t){
        .cycles = (int64_t)buf.values[0],
        .instructions = (int64_t)buf.values[1],
    };
}

void talp_perf_syscall_close(talp_perf_group_t *group) {

    for (int i = 0; i < 2; ++i) {
        if (group->fds[i] >= 0) {
            close(group->fds[i]);
            group->fds[i] = -1;
        }
    }
}
