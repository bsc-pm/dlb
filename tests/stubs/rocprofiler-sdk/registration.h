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

#ifndef REGISTRATION_H
#define REGISTRATION_H

#include <stddef.h>
#include <stdint.h>

typedef struct rocprofiler_client_id_t
{
    const char*    name;
    const uint32_t handle;
} rocprofiler_client_id_t;

typedef void (*rocprofiler_client_finalize_t)(rocprofiler_client_id_t);

typedef int (*rocprofiler_tool_initialize_t)(rocprofiler_client_finalize_t finalize_func,
                                             void*                         tool_data);

typedef void (*rocprofiler_tool_finalize_t)(void* tool_data);

typedef struct rocprofiler_tool_configure_result_t
{
    size_t                        size;
    rocprofiler_tool_initialize_t initialize;
    rocprofiler_tool_finalize_t   finalize;
    void*                         tool_data;
} rocprofiler_tool_configure_result_t;

typedef rocprofiler_tool_configure_result_t*
    (*rocprofiler_configure_fn)(uint32_t, const char*, uint32_t, rocprofiler_client_id_t*);

#endif /* REGISTRATION_H */
