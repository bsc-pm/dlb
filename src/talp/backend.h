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

#ifndef BACKEND_H
#define BACKEND_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { DLB_BACKEND_ABI_VERSION = 3 };

/* Backends should use these error codes for returning */
enum DLB_Backend_Error_codes {
    DLB_BACKEND_SUCCESS = 0,
    DLB_BACKEND_ERROR   = 1,
};

typedef struct gpu_device_entry_t {
    uint32_t local_id;
    uint64_t node_unique_id;
} gpu_device_entry_t;

typedef struct gpu_timers_t {
    int64_t useful;
    int64_t communication;
} gpu_timers_t;

typedef struct hw_counters_t {
    int64_t cycles;
    int64_t instructions;
} hw_counters_t;

/* Backend to Core communication interface */
typedef struct {
    uint32_t abi_version;
    uint32_t struct_size;

    struct {
        void (*enter_runtime)(void);
        void (*exit_runtime)(void);
    } gpu;

} core_api_t;


/* Core to Backend communication interface */
typedef struct {
    uint32_t abi_version;
    uint32_t struct_size;

    const char *name;

    struct {
        bool gpu:1;
        bool gpu_amd:1;
        bool gpu_nvidia:1;
        bool hwc:1;
    } capabilities;

    int (*probe)(void);
    int (*init)(const core_api_t*);
    int (*start)(void);
    int (*stop)(void);
    int (*finalize)(void);

    struct {
        int (*collect)(gpu_timers_t *out, size_t capacity, uint64_t *out_mask);
        int (*get_devices)(gpu_device_entry_t *out, size_t capacity, size_t *out_count);
        int (*get_uuids)(char *buffer, size_t buffer_size, bool full_uuid);
    } gpu;

    struct {
        int (*collect)(hw_counters_t *out);
    } hwc;

} backend_api_t;


/* Backends must define and export this function */
backend_api_t* DLB_Get_Backend_API(void);


#ifdef __cplusplus
}
#endif

#endif /* BACKEND_H */
