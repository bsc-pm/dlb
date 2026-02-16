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

#ifndef PLUGIN_MANAGER_H
#define PLUGIN_MANAGER_H

#include "backend.h"
#include <stdbool.h>
#include <stddef.h>

extern const core_api_t core_api;

const backend_api_t*
      talp_backend_manager_load_gpu_backend(const char *name);
void  talp_backend_manager_unload_gpu_backend(void);

const backend_api_t*
      talp_backend_manager_load_hwc_backend(const char *name);
void  talp_backend_manager_unload_hwc_backend(void);

void* talp_backend_manager_get_symbol_from_plugin(const char *symbol, const char *plugin_name);
int   talp_backend_manager_get_gpu_affinity(char *buffer, size_t buffer_size, bool full_uuid);


#endif /* PLUGIN_MANAGER_H */
