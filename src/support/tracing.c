/*********************************************************************************/
/*  Copyright 2017 Barcelona Supercomputing Center                               */
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

#ifdef INSTRUMENTATION_VERSION

#include "support/tracing.h"
#include "support/options.h"
#include <stdio.h>

// Extrae API calls
void Extrae_event(unsigned type, long long value) __attribute__((weak));
void Extrae_eventandcounters(unsigned type, long long value) __attribute__((weak));
void Extrae_define_event_type(unsigned *type, char *type_description, int *nvalues,
                               long long *values, char **values_description) __attribute__((weak));

static bool tracing_initialized = false;

// Pointer to store the function to call
static void (*extrae_set_event) (unsigned type, long long value);

static void dummy (unsigned type, long long value) {}

void add_event( unsigned type, long long value ) {
    extrae_set_event( type, value );
}

void init_tracing(const options_t *options) {
    if (tracing_initialized) return;
    tracing_initialized = true;

    if (options->trace_enabled && Extrae_event &&
            Extrae_eventandcounters && Extrae_define_event_type ) {

        // Set up function
        if ( options->trace_counters ) {
            extrae_set_event = Extrae_eventandcounters;
        } else {
            extrae_set_event = Extrae_event;
        }

        unsigned type;
        int n_values;
        long long values[12]= {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};

        //THREADS_USED_EVENT
        type=THREADS_USED_EVENT;
        n_values=0;
        Extrae_define_event_type(&type, "DLB Used threads", &n_values, NULL, NULL);

        //RUNTIME_EVENT
        type=RUNTIME_EVENT;
        n_values=12;
        char* value_desc[12]= {"User code", "Init", "Into MPI call", "Out of MPI call", "Lend",
            "Reclaim", "Acquire", "Borrow", "Return", "Reset DLB", "Barrier", "PollDROM"};
        Extrae_define_event_type(&type, "DLB Runtime call", &n_values, values, value_desc);

        //IDLE_CPUS_EVENT
        type=IDLE_CPUS_EVENT;
        n_values=0;
        Extrae_define_event_type(&type, "DLB Idle cpus", &n_values, NULL, NULL);

        //ITERATION_EVENT
        type=ITERATION_EVENT;
        n_values=0;
        Extrae_define_event_type(&type, "DLB num iteration detected", &n_values, NULL, NULL);

        //DLB_MODE_EVENT
        type=DLB_MODE_EVENT;
        n_values=4;
        char* value_desc2[4]= {"not ready", "Enabled", "Disabled", "Single"};
        Extrae_define_event_type(&type, "DLB mode", &n_values, values, value_desc2);
    } else {
        extrae_set_event = dummy;
    }
}
#endif
