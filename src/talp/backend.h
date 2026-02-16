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

enum { DLB_BACKEND_ABI_VERSION = 1 };

/* Backends should use these error codes for returning */
enum DLB_Backend_Error_codes {
    DLB_BACKEND_SUCCESS = 0,
    DLB_BACKEND_ERROR   = 1,
};


typedef struct gpu_measurements {
    int64_t useful_time;
    int64_t communication_time;
    int64_t inactive_time;
} gpu_measurements_t;

typedef struct hwc_measurements {
    int64_t cycles;
    int64_t instructions;
} hwc_measurements_t;

/* Backend to Core communication interface */
typedef struct {
    uint32_t abi_version;
    uint32_t struct_size;

    struct {
        void (*enter_runtime)(void);
        void (*exit_runtime)(void);
        void (*submit_measurements)(const gpu_measurements_t*);
    } gpu;

    struct {
        void (*submit_measurements)(const hwc_measurements_t*);
    } hwc;

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

    void (*flush)(void);

    int (*get_gpu_affinity)(char *buffer, size_t buffer_size, bool full_uuid);

} backend_api_t;


/* Backends must define and export this function */
backend_api_t* DLB_Get_Backend_API(void);


#ifdef __cplusplus
}
#endif

#endif /* BACKEND_H */
