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

/********* EXTRAE EVENTS *************/
typedef enum InstrumentEvent {
    THREADS_USED_EVENT  = 800000,
    RUNTIME_EVENT       = 800020,
    IDLE_CPUS_EVENT     = 800030,
    GIVE_CPUS_EVENT     = 800031,
    WANT_CPUS_EVENT     = 800032,
    MAX_PAR_EVENT       = 800033,
    ITERATION_EVENT     = 800040,
    DLB_MODE_EVENT      = 800050,
    REBIND_EVENT        = 800060,
    BINDINGS_EVENT      = 800061,
    CALLBACK_EVENT      = 800070,
    LOOP_STATE          = 800080,
    MONITOR_REGION      = 800100,
    MONITOR_STATE       = 800101,
    MONITOR_CYCLES      = 800110,
    MONITOR_INSTR       = 800111,
} instrument_event_t;

typedef enum InstrumentRuntimeValue {
    /* EVENT_USER              = 0,  (deprecated) */
    EVENT_INIT              = 1,
    EVENT_INTO_MPI          = 2,
    EVENT_OUTOF_MPI         = 3,
    EVENT_LEND              = 4,
    EVENT_RECLAIM           = 5,
    EVENT_ACQUIRE           = 6,
    EVENT_BORROW            = 7,
    EVENT_RETURN            = 8,
    /* EVENT_RESET             = 9,  (deprecated) */
    EVENT_BARRIER           = 10,
    EVENT_POLLDROM          = 11,
    EVENT_FINALIZE          = 12,
    EVENT_MAX_PARALLELISM   = 13,
} instrument_runtime_value_t;

typedef enum InstrumentModeValue {
    EVENT_ENABLED       = 1,
    EVENT_DISABLED      = 2,
    EVENT_SINGLE        = 3,
} instrument_mode_value_t;

typedef enum InstrumentRegionState {
    MONITOR_STATE_DISABLED = 0,
    MONITOR_STATE_USEFUL = 1,
    MONITOR_STATE_NOT_USEFUL_MPI = 2,
    MONITOR_STATE_NOT_USEFUL_OMP_IN = 3,
    MONITOR_STATE_NOT_USEFUL_OMP_OUT = 4,
    MONITOR_STATE_NOT_USEFUL_GPU = 5,
} instrument_region_state_t;

typedef enum InstrumentAction {
    EVENT_BEGIN,
    EVENT_END,
    EVENT_BEGINEND,
} instrument_action_t;

/*************************************/

#ifdef INSTRUMENTATION_VERSION
#include "support/options.h"
void instrument_register_event(unsigned type, long long value, const char *value_description);
void instrument_event(instrument_event_t type, long long value, instrument_action_t action);
void instrument_nevent(unsigned count, instrument_event_t *types, long long *values);
void add_event(unsigned type, long long value);
void init_tracing(const options_t *options);
void instrument_finalize(void);
void instrument_print_flags(void);
#else
#define instrument_register_event(type, value, value_description)
#define instrument_event(type, value, action)
#define instrument_nevent(count, types, values)
#define add_event(type, value)
#define init_tracing(options)
#define instrument_finalize()
#define instrument_print_flags()
#endif


#ifdef INSTRUMENTATION_VERSION
#define DLB_INSTR(f) f
#else
#define DLB_INSTR(f)
#endif
