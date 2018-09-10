/*********************************************************************************/
/*  Copyright 2009-2018 Barcelona Supercomputing Center                          */
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

#ifndef PERISH_H
#define PERISH_H

#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>

/*
 * Any source file that includes this header will kill itself after PERISH_SECONDS
 */
enum { PERISH_SECONDS = 60 };

static void termination_handler(int signum) {
    fprintf(stderr, "*** PROCESS HAS LASTED MORE THAN %d SECONDS. KILLING ITSELF ****\n",
            PERISH_SECONDS);
    raise(SIGKILL);
}

static int is_debugger_present(void) {
    char buf[1024];
    int debugger_present = 0;
    int fd = open("/proc/self/status", O_RDONLY);
    ssize_t num_read = read(fd, buf, sizeof(buf)-1);
    if (num_read > 0) {
        buf[num_read] = 0;
        static const char TracerPid[] = "TracerPid:";
        char *tracer_pid = strstr(buf, TracerPid);
        if (tracer_pid) {
            debugger_present = !!atoi(tracer_pid + sizeof(TracerPid) - 1);
        }
    }
    return debugger_present;
}

__attribute__((constructor)) static void perish(void) {
    if (!is_debugger_present()) {
        struct sigaction new_action;
        new_action.sa_handler = termination_handler;
        sigemptyset(&new_action.sa_mask);
        new_action.sa_flags = 0;
        sigaction(SIGALRM, &new_action, NULL);
        alarm(PERISH_SECONDS);
    }
}

#endif /* PERISH_H */
