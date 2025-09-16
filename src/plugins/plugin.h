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

#ifndef PLUGIN_H
#define PLUGIN_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Plugins should use these error codes for returning */
enum DLB_Plugin_Error_codes {
    DLB_PLUGIN_SUCCESS = 0,
    DLB_PLUGIN_ERROR   = 1,
};


typedef struct {
    const char *name;
    int version;
} plugin_info_t;


/* Plugins may implement these functions */
typedef int (*plugin_init_func_t)(plugin_info_t *info);
typedef int (*plugin_finalize_func_t)(void);
typedef int (*plugin_get_affinity_func_t)(char *buffer, size_t buffer_size, bool full_uuid);

typedef struct {
    plugin_init_func_t          init;
    plugin_finalize_func_t      finalize;
    plugin_get_affinity_func_t  get_affinity;
} plugin_api_t;


/* Plugins must define and export this function */
plugin_api_t* DLB_Get_Plugin_API(void);


/* typedef for the plugin manager */
typedef plugin_api_t* (*plugin_get_api_func_t)(void);


#ifdef __cplusplus
}
#endif

#endif /* PLUGIN_H */
