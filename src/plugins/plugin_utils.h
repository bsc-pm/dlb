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

#ifndef PLUGIN_UTILS_H
#define PLUGIN_UTILS_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>


static inline int64_t get_timestamp(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000000000LL + t.tv_nsec;
}


static inline int plugin_is_verbose() {
    static int initialized = 0;
    static int verbose = 0;

    if (!initialized) {
        const char *env = getenv("DLB_PLUGIN_VERBOSE");
        if (env && (*env == '1' || *env == 'y' || *env == 'Y')) {
            verbose = 1;
        }
        initialized = 1;
    }
    return verbose;
}

#define PLUGIN_PRINT(fmt, ...) \
    do { if (plugin_is_verbose()) fprintf(stderr, "[DLB PLUGIN] " fmt, ##__VA_ARGS__); } while (0)


#endif /* PLUGIN_UTILS_H */

