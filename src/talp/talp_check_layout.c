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

#include "apis/dlb_talp.h"
#include "talp/talp_types.h"
#include <stddef.h>
#include <stdint.h>


/*** dlb_monitor_t ***/

size_t sizeof_dlb_monitor_t(void) {
    return sizeof(dlb_monitor_t);
}

#define DECLARE_OFFSET_FN(name, ...) \
    size_t offset_dlb_monitor_t_##name(void) { return offsetof(dlb_monitor_t, name); }
FOR_DLB_MONITOR_FIELDS(DECLARE_OFFSET_FN)
#undef DECLARE_OFFSET_FN

int check_internal_dlb_monitor_t(void) {
    typedef struct {
        #define DEFINE_C_TYPES(name, c_type, ...) c_type name;
        FOR_DLB_MONITOR_FIELDS(DEFINE_C_TYPES)
        #undef DEFINE_C_TYPES
    } internal_dlb_monitor_t;
    return sizeof(dlb_monitor_t) == sizeof(internal_dlb_monitor_t) ? 0 : 1;
}


/*** dlb_pop_metrics_t ***/

size_t sizeof_dlb_pop_metrics_t(void) {
    return sizeof(dlb_pop_metrics_t);
}

// array type not in macro
size_t offset_dlb_pop_metrics_t_name(void) { return offsetof(dlb_pop_metrics_t, name); }

#define DECLARE_OFFSET_FN(name, ...) \
    size_t offset_dlb_pop_metrics_t_##name(void) { return offsetof(dlb_pop_metrics_t, name); }
FOR_DLB_POP_METRICS_FIELDS(DECLARE_OFFSET_FN, DECLARE_OFFSET_FN)
#undef DECLARE_OFFSET_FN

int check_internal_dlb_pop_metrics_t(void) {
    typedef struct {
        char name[DLB_MONITOR_NAME_MAX];
        #define DEFINE_C_TYPES(name, c_type, ...) c_type name;
        FOR_DLB_POP_METRICS_FIELDS(DEFINE_C_TYPES, DEFINE_C_TYPES)
        #undef DEFINE_C_TYPES
    } internal_dlb_pop_metrics_t;
    return sizeof(dlb_pop_metrics_t) == sizeof(internal_dlb_pop_metrics_t) ? 0 : 1;
}


/*** dlb_node_metrics_t ***/

size_t sizeof_dlb_node_metrics_t(void) {
    return sizeof(dlb_node_metrics_t);
}

// array type not in macro
size_t offset_dlb_node_metrics_t_name(void) { return offsetof(dlb_node_metrics_t, name); }

#define DECLARE_OFFSET_FN(name, ...) \
    size_t offset_dlb_node_metrics_t_##name(void) { return offsetof(dlb_node_metrics_t, name); }
FOR_DLB_NODE_METRICS_FIELDS(DECLARE_OFFSET_FN)
#undef DECLARE_OFFSET_FN

int check_internal_dlb_node_metrics_t(void) {
    typedef struct {
        char name[DLB_MONITOR_NAME_MAX];
        #define DEFINE_C_TYPES(name, c_type, ...) c_type name;
        FOR_DLB_NODE_METRICS_FIELDS(DEFINE_C_TYPES)
        #undef DEFINE_C_TYPES
    } internal_dlb_node_metrics_t;
    return sizeof(dlb_node_metrics_t) == sizeof(internal_dlb_node_metrics_t) ? 0 : 1;
}
