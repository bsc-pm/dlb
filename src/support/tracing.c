/*********************************************************************************/
/*  Copyright 2009-2019 Barcelona Supercomputing Center                          */
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

#ifdef INSTRUMENTATION_VERSION

#include "support/tracing.h"
#include "support/options.h"
#include "support/debug.h"

// Extrae API calls
void Extrae_event(unsigned type, long long value) __attribute__((weak));
void Extrae_eventandcounters(unsigned type, long long value) __attribute__((weak));
void Extrae_define_event_type(unsigned *type, char *type_description, int *nvalues,
                               long long *values, char **values_description) __attribute__((weak));

static bool tracing_initialized = false;
static instrument_events_t instrument = INST_NONE;

static void dummy (unsigned type, long long value) {}

static void (*extrae_set_event) (unsigned type, long long value) = dummy;

void instrument_event(unsigned type, long long value, instrument_action_t action) {
    switch(type) {
        case RUNTIME_EVENT:
            switch(value) {
                case EVENT_INIT:
                case EVENT_FINALIZE:
                    if (instrument != INST_NONE) {
                        extrae_set_event(type, action == EVENT_BEGIN ? value : 0);
                    }
                    break;
                case EVENT_INTO_MPI:
                case EVENT_OUTOF_MPI:
                    if (instrument & INST_MPI) {
                        extrae_set_event(type, action == EVENT_BEGIN ? value : 0);
                    }
                    break;
                case EVENT_LEND:
                case EVENT_RECLAIM:
                case EVENT_ACQUIRE:
                case EVENT_BORROW:
                case EVENT_RETURN:
                    if (instrument & INST_LEWI) {
                        extrae_set_event(type, action == EVENT_BEGIN ? value : 0);
                    }
                    break;
                case EVENT_BARRIER:
                    if (instrument & INST_BARR) {
                        extrae_set_event(type, action == EVENT_BEGIN ? value : 0);
                    }
                    break;
                case EVENT_POLLDROM:
                    if (instrument == INST_ALL) {
                        extrae_set_event(type, action == EVENT_BEGIN ? value : 0);
                    }
                    break;
            }
            break;
        case IDLE_CPUS_EVENT:
        case GIVE_CPUS_EVENT:
        case WANT_CPUS_EVENT:
            if (instrument & INST_CPUS) {
                extrae_set_event(type, action == EVENT_BEGIN ? value : 0);
            }
            break;
        case REBIND_EVENT:
        case BINDINGS_EVENT:
            if (instrument & INST_OMPT) {
                extrae_set_event(type, action == EVENT_BEGIN ? value : 0);
            }
            break;
        case MONITOR_REGION:
            if (instrument & INST_TALP) {
                extrae_set_event(type, action == EVENT_BEGIN ? value : 0);
            }
            break;
        default:
            if (instrument != INST_NONE) {
                extrae_set_event(type, action == EVENT_BEGIN ? value : 0);
            }
            break;
    }
}


void add_event( unsigned type, long long value ) {
    extrae_set_event( type, value );
}

void init_tracing(const options_t *options) {
    if (tracing_initialized) return;
    tracing_initialized = true;
    instrument = options->instrument;

    if (instrument && Extrae_event &&
            Extrae_eventandcounters && Extrae_define_event_type ) {

        // Set up function
        if ( options->instrument_counters ) {
            extrae_set_event = Extrae_eventandcounters;
        } else {
            extrae_set_event = Extrae_event;
        }

        unsigned type;
        int n_values;
        long long values[13]= {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};

        //THREADS_USED_EVENT
        type=THREADS_USED_EVENT;
        n_values=0;
        Extrae_define_event_type(&type, "DLB Used threads", &n_values, NULL, NULL);

        //LOOP_STATE
        type=LOOP_STATE;
        n_values=6;
         char * value_loop[6] = { "NO_LOOP", "IN_LOOP", "NEW_ITERATION","NEW_LOOP","END_NEW_LOOP","END_LOOP"};
        Extrae_define_event_type(&type, "Dynais State", &n_values, values, value_loop);

         //MONITOR_REGION
        type=MONITOR_REGION;
        n_values=2;
         char * value_regions[2] = { "Disabled", "Enabled"};
        Extrae_define_event_type(&type, "MonitorRegions mode", &n_values, values, value_regions);


        //RUNTIME_EVENT
        type=RUNTIME_EVENT;
        n_values=13;
        char* value_desc[13]= {"User code", "Init", "Into MPI call", "Out of MPI call", "Lend",
            "Reclaim", "Acquire", "Borrow", "Return", "Reset DLB", "Barrier", "PollDROM",
            "Finalize"};
        Extrae_define_event_type(&type, "DLB Runtime call", &n_values, values, value_desc);

        //IDLE_CPUS_EVENT
        type=IDLE_CPUS_EVENT;
        n_values=0;
        Extrae_define_event_type(&type, "DLB Idle cpus", &n_values, NULL, NULL);

        //GIVE_CPUS_EVENT
        type=GIVE_CPUS_EVENT;
        n_values=0;
        Extrae_define_event_type(&type, "DLB Give Number of CPUs", &n_values, NULL, NULL);

        //WANT_CPUS_EVENT
        type=WANT_CPUS_EVENT;
        n_values=0;
        Extrae_define_event_type(&type, "DLB Want Number of CPUs", &n_values, NULL, NULL);

        //ITERATION_EVENT
        type=ITERATION_EVENT;
        n_values=0;
        Extrae_define_event_type(&type, "DLB num iteration detected", &n_values, NULL, NULL);

        //DLB_MODE_EVENT
        type=DLB_MODE_EVENT;
        n_values=4;
        char* value_desc2[4]= {"not ready", "Enabled", "Disabled", "Single"};
        Extrae_define_event_type(&type, "DLB mode", &n_values, values, value_desc2);

        //REBIND_EVENT
        type=REBIND_EVENT;
        n_values=0;
        Extrae_define_event_type(&type, "DLB thread rebind in OMPT", &n_values, NULL, NULL);

        //BINDINGS_EVENT
        type=BINDINGS_EVENT;
        n_values=0;
        Extrae_define_event_type(&type, "DLB thread binding in OMPT", &n_values, NULL, NULL);
    } else {
        extrae_set_event = dummy;
    }
}

void tracing_print_flags(void) {
    info0("Tracing options: %s", instrument_events_tostr(instrument));
}

#endif
