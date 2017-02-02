/*********************************************************************************/
/*  Copyright 2015 Barcelona Supercomputing Center                               */
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
/*  along with DLB.  If not, see <http://www.gnu.org/licenses/>.                 */
/*********************************************************************************/

#ifndef TYPES_H
#define TYPES_H

#include <stdbool.h>

typedef enum BlockingMode {
    ONE_CPU, // MPI not set to blocking, leave a cpu while in a MPI blockin call
    BLOCK,   // MPI set to blocking mode
} blocking_mode_t;

typedef enum VerboseOptions {
    VB_CLEAR    = 0,
    VB_API      = 1 << 0,
    VB_MICROLB  = 1 << 1,
    VB_SHMEM    = 1 << 2,
    VB_MPI_API  = 1 << 3,
    VB_MPI_INT  = 1 << 4,
    VB_STATS    = 1 << 5,
    VB_DROM     = 1 << 6
} verbose_opts_t;

typedef enum VerboseFormat {
    VBF_CLEAR   = 0,
    VBF_NODE    = 1 << 0,
    VBF_PID     = 1 << 1,
    VBF_MPINODE = 1 << 2,
    VBF_MPIRANK = 1 << 3,
    VBF_THREAD  = 1 << 4
} verbose_fmt_t;

typedef enum DebugOptions {
    DBG_CLEAR        = 0,
    DBG_REGSIGNALS   = 1 << 0,
    DBG_RETURNSTOLEN = 1 << 1
} debug_opts_t;

typedef enum PriorityType {
    PRIO_NONE,
    PRIO_AFFINITY_FIRST,
    PRIO_AFFINITY_FULL,
    PRIO_AFFINITY_ONLY
} priority_t;

typedef enum PolicyType {
    POLICY_NONE,
    POLICY_LEWI,
    POLICY_WEIGHT,
    POLICY_LEWI_MASK,
    POLICY_AUTO_LEWI_MASK
} policy_t;

void parse_bool(const char *str, bool *value);
void parse_int(const char *str, int *value);
void parse_blocking_mode(const char *str, blocking_mode_t *value);
void parse_verbose_opts(const char *str, verbose_opts_t *value);
void parse_verbose_fmt(const char *str, verbose_fmt_t *value);
void parse_debug_opts(const char *str, debug_opts_t *value);
void parse_priority(const char *str, priority_t *value);
void parse_policy(const char *str, policy_t *value);
const char* policy_tostr(policy_t policy);

#endif /* TYPES_H */
