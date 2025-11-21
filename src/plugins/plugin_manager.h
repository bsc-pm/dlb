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

#include <stdbool.h>
#include <stddef.h>

/* Un/load specific plugin or return error */
int  load_plugin(const char *plugin_name);
int  unload_plugin(const char *plugin_name);

/* Best effort for un/loading comma-separated list of plugins via --plugin=... */
void load_plugins(const char *plugin_list);
void unload_plugins(const char *plugin_list);

void plugin_get_gpu_affinity(char *buffer, size_t buffer_size, bool full_uuid);

void* plugin_get_symbol_from_plugin(const char *symbol, const char *plugin_name);

#endif /* PLUGIN_MANAGER_H */
