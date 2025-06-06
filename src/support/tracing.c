/*********************************************************************************/
/*  Copyright 2009-2024 Barcelona Supercomputing Center                          */
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
#include "support/gtree.h"

#include <string.h>
#include <inttypes.h>

// Extrae API calls
void Extrae_event(unsigned type, long long value) __attribute__((weak));
void Extrae_eventandcounters(unsigned type, long long value) __attribute__((weak));
void Extrae_nevent(
        unsigned count, unsigned *types, long long *values) __attribute__((weak));
void Extrae_neventandcounters(
        unsigned count, unsigned *types, long long *values) __attribute__((weak));
void Extrae_define_event_type(unsigned *type, char *type_description, int *nvalues,
                               long long *values, char **values_description) __attribute__((weak));
void Extrae_change_num_threads(unsigned n) __attribute__((weak));

static bool tracing_initialized = false;
static instrument_items_t instrument = INST_NONE;

static void dummy(unsigned type, long long value) {}
static void dummy_n(unsigned count, unsigned *types, long long *values) {}

static void (*extrae_set_event) (unsigned type, long long value) = dummy;
static void (*extrae_set_nevent) (unsigned count, unsigned *types, long long *values) = dummy_n;

/* Event name dictionary */
static char* get_event_name(unsigned int type) {
    switch(type) {
        case MONITOR_REGION: return "Monitor Regions";
    }
    return NULL;
}


/* GTree containing each dynamic Event description. <key> is event_type, <value> is event_info_t */
typedef struct EventInfo {
    char *name;
    int nvalues;
    long long *values;
    char **values_description;
} event_info_t;
static GTree *event_tree = NULL;

static gint key_compare_func(gconstpointer a, gconstpointer b) {
    return (uintptr_t)a < (uintptr_t)b ? -1 : (uintptr_t)a > (uintptr_t)b;
}

/* Define Extrae custom type based on a dynamic event */
static gint extrae_add_definitions(gpointer key, gpointer value, gpointer data) {
    unsigned int type = (uintptr_t)key;
    event_info_t *event = value;

    Extrae_define_event_type(&type, event->name,
            &event->nvalues, event->values, event->values_description);

    /* return false to not stop traversing */
    return false;
}

/* De-allocate event info */
static void destroy_node(gpointer value) {
    event_info_t *event = value;
    int i;
    for (i=0; i<event->nvalues; ++i) {
        free(event->values_description[i]);
        event->values_description[i] = NULL;
    }
    free(event->values_description);
    event->values_description = NULL;
    free(event->values);
    event->values = NULL;
    event->nvalues = 0;
    free(event);
}

void instrument_register_event(unsigned int type, long long value, const char *value_description) {

    enum { EVENT_NAME_MAX_LEN = 128 };

    /* Allocate GTree if needed */
    if (event_tree == NULL) {
        event_tree = g_tree_new_full(
                (GCompareDataFunc)key_compare_func,
                NULL, NULL, destroy_node);
    }

    /* Get event */
    gpointer key = (void*)(uintptr_t)type;
    event_info_t *event = g_tree_lookup(event_tree, key);

    /* If event does not exist, allocate new node */
    if (event == NULL ) {
        event = malloc(sizeof(event_info_t));
        event->name = get_event_name(type);
        event->nvalues = 0;
        event->values = NULL;
        event->values_description = NULL;
        g_tree_insert(event_tree, key, event);
    }

    /* Increment number of values */
    ++event->nvalues;

    /* Add value */
    void *p = realloc(event->values, sizeof(long long) * event->nvalues);
    fatal_cond(!p, "realloc failed");
    event->values = p;
    event->values[event->nvalues-1] = value;

    /* Add value description */
    p = realloc(event->values_description, sizeof(char*) * event->nvalues);
    fatal_cond(!p, "realloc failed");
    event->values_description = p;
    size_t desc_len = strnlen(value_description, EVENT_NAME_MAX_LEN);
    char *desc = malloc(sizeof(char)*desc_len);
    snprintf(desc, EVENT_NAME_MAX_LEN, "%s", value_description);
    event->values_description[event->nvalues-1] = desc;
}


void instrument_event(instrument_event_t type, long long value, instrument_action_t action) {
    switch(type) {
        case RUNTIME_EVENT:
            switch((instrument_runtime_value_t)value) {
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
                case EVENT_MAX_PARALLELISM:
                    if (instrument == INST_ALL) {
                        extrae_set_event(type, action == EVENT_BEGIN ? value : 0);
                    }
                    break;
            }
            break;
        case IDLE_CPUS_EVENT:
        case GIVE_CPUS_EVENT:
        case WANT_CPUS_EVENT:
        case MAX_PAR_EVENT:
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
        case CALLBACK_EVENT:
            if (instrument & INST_CBCK) {
                extrae_set_event(type, action == EVENT_BEGIN ? value : 0);
            }
            break;
        case MONITOR_REGION:
        case MONITOR_STATE:
            if (instrument & INST_TALP) {
                extrae_set_event(type, action == EVENT_BEGIN ? value : 0);
            }
            break;
        case MONITOR_CYCLES:
        case MONITOR_INSTR:
            if (instrument & INST_TALP) {
                extrae_set_event(type, value);
            }
            break;
        case THREADS_USED_EVENT:
        case ITERATION_EVENT:
        case DLB_MODE_EVENT:
        case LOOP_STATE:
            if (instrument != INST_NONE) {
                extrae_set_event(type, action == EVENT_BEGIN ? value : 0);
            }
            break;
    }
}

void instrument_nevent(unsigned count, instrument_event_t *types, long long *values) {
    extrae_set_nevent(count, types, values);
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
            extrae_set_nevent = Extrae_neventandcounters;
        } else {
            extrae_set_event = Extrae_event;
            extrae_set_nevent = Extrae_nevent;
        }

        unsigned type;
        int n_values;
        int zero_values = 0;
        /* Note: RUNTIME_EVENT currently holds the maximum number of events. */
        long long values[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13};

        #define CHECK_VALUE_DESCRIPTIONS(type, desc) \
            static_ensure( \
                    sizeof(desc) / sizeof(desc[0]) <= sizeof(values) / sizeof(values[0]), \
                    "values array contents does match " type " descriptions");

        // THREADS_USED_EVENT
        type = THREADS_USED_EVENT;
        Extrae_define_event_type(&type, "DLB Used threads", &zero_values, NULL, NULL);

        // RUNTIME_EVENT
        type = RUNTIME_EVENT;
        char* value_desc[] = {"User code", "Init", "Into MPI call", "Out of MPI call", "Lend",
            "Reclaim", "Acquire", "Borrow", "Return", "Reset DLB", "Barrier", "PollDROM",
            "Finalize", "Set Max Parallelism"};
        n_values = sizeof(value_desc) / sizeof(value_desc[0]);
        Extrae_define_event_type(&type, "DLB Runtime call", &n_values, values, value_desc);

        CHECK_VALUE_DESCRIPTIONS("RUNTIME_EVENT", value_desc);

        // IDLE_CPUS_EVENT
        type = IDLE_CPUS_EVENT;
        Extrae_define_event_type(&type, "DLB Idle cpus", &zero_values, NULL, NULL);

        // GIVE_CPUS_EVENT
        type = GIVE_CPUS_EVENT;
        Extrae_define_event_type(&type, "DLB Give Number of CPUs", &zero_values, NULL, NULL);

        // WANT_CPUS_EVENT
        type = WANT_CPUS_EVENT;
        Extrae_define_event_type(&type, "DLB Want Number of CPUs", &zero_values, NULL, NULL);

        // MAX_PAR_EVENT
        type = MAX_PAR_EVENT;
        Extrae_define_event_type(&type, "DLB Max parallelism", &zero_values, NULL, NULL);

        // ITERATION_EVENT
        type = ITERATION_EVENT;
        Extrae_define_event_type(&type, "DLB num iteration detected", &zero_values, NULL, NULL);

        // DLB_MODE_EVENT
        type = DLB_MODE_EVENT;
        char *mode_desc[] = {"not ready", "Enabled", "Disabled", "Single"};
        n_values = sizeof(mode_desc) / sizeof(mode_desc[0]);
        Extrae_define_event_type(&type, "DLB mode", &n_values, values, mode_desc);

        CHECK_VALUE_DESCRIPTIONS("DLB_MODE_EVENT", mode_desc);

        // REBIND_EVENT
        type = REBIND_EVENT;
        Extrae_define_event_type(&type, "DLB thread rebind in OMPT", &zero_values, NULL, NULL);

        // BINDINGS_EVENT
        type = BINDINGS_EVENT;
        Extrae_define_event_type(&type, "DLB thread binding in OMPT", &zero_values, NULL, NULL);

        // CALLBACK_EVENT
        type = CALLBACK_EVENT;
        char *callbacks_desc[] = {"outside callback", "set_num_threads", "set_active_mask",
            "set_process_mask", "add_active_mask", "add_process_mask", "enable_cpu",
            "disable_cpu", "enable_cpu_set", "disable_cpu_set"};
        n_values = sizeof(callbacks_desc) / sizeof(callbacks_desc[0]);
        Extrae_define_event_type(&type, "DLB callback", &n_values, values, callbacks_desc);

        CHECK_VALUE_DESCRIPTIONS("CALLBACK_EVENT", callbacks_desc);

        // LOOP_STATE
        type = LOOP_STATE;
        char *loop_desc[] = {"NO_LOOP", "IN_LOOP",
             "NEW_ITERATION","NEW_LOOP","END_NEW_LOOP","END_LOOP"};
        n_values = sizeof(loop_desc) / sizeof(loop_desc[0]);
        Extrae_define_event_type(&type, "Dynais State", &n_values, values, loop_desc);

        CHECK_VALUE_DESCRIPTIONS("LOOP_STATE", loop_desc);

        // MONITOR_REGION
        type = MONITOR_REGION;
        Extrae_define_event_type(&type, get_event_name(type), &zero_values, NULL, NULL);

        // MONITOR_STATE
        type = MONITOR_STATE;
        char *monitor_state_desc[] = {"Disabled", "Useful", "Not useful MPI", "Not useful OMP in",
            "Not useful OMP out"};
        n_values = sizeof(monitor_state_desc) / sizeof(monitor_state_desc[0]);
        Extrae_define_event_type(&type, "DLB Region state", &n_values, values, monitor_state_desc);

        CHECK_VALUE_DESCRIPTIONS("MONITOR_STATE", monitor_state_desc);

        // MONITOR_CYCLES
        type = MONITOR_CYCLES;
        Extrae_define_event_type(&type, "DLB Region cycles", &zero_values, NULL, NULL);

        // MONITOR_INSTR
        type = MONITOR_INSTR;
        Extrae_define_event_type(&type, "DLB Region instructions", &zero_values, NULL, NULL);

        #undef CHECK_VALUE_DESCRIPTIONS

        verbose(VB_INSTR, "Instrumentation successfully initialized");
    } else {
        if (instrument == INST_NONE) {
            verbose(VB_INSTR, "Instrumentation is explicitly disabled.");
        } else {
            verbose(VB_INSTR, "DLB cannot find the Extrae library. Instrumentation is disabled.");
            instrument = INST_NONE;
        }
        tracing_initialized = false;
        extrae_set_event = dummy;
    }

    if (options->instrument_extrae_nthreads > 0 && Extrae_change_num_threads) {
        info0("Increasing the Extrae buffer to %d threads\n", options->instrument_extrae_nthreads);
        Extrae_change_num_threads(options->instrument_extrae_nthreads);
    }
}

void instrument_finalize(void) {
    if (tracing_initialized) {
        verbose(VB_INSTR, "Finalizing instrumentation");
        tracing_initialized = false;
        g_tree_foreach(event_tree, extrae_add_definitions, NULL);
    }
    g_tree_destroy(event_tree);
    event_tree = NULL;
}

void instrument_print_flags(void) {
    info0("Tracing options: %s", instrument_items_tostr(instrument));
}

#endif
