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

#include "papi.h"
#include <stdbool.h>
#include <stddef.h>

typedef struct event_set_t {
    int id;
    int num_events;
    int times_called;
    bool initialized;
    bool started;
} event_set_t;

static __thread event_set_t unique_event_set = {
    .id = 1,
    .num_events = 0,
    .times_called = 0,
    .initialized = false,
    .started = false,
};

int PAPI_library_init(int version) {
    return PAPI_VER_CURRENT;
}

int PAPI_thread_init(unsigned long (*id_fn) (void)) {
    return PAPI_OK;
}

int PAPI_register_thread(void) {
    return PAPI_OK;
}

int PAPI_is_initialized(void) {
    return PAPI_LOW_LEVEL_INITED;
}

int PAPI_unregister_thread(void) {
    return PAPI_OK;
}

void PAPI_shutdown(void) {
}

int PAPI_create_eventset(int *EventSet) {

    if (EventSet == NULL) return PAPI_EINVAL;
    if (*EventSet != PAPI_NULL) return PAPI_EINVAL;
    if (unique_event_set.initialized) return PAPI_ENOMEM;

    unique_event_set.initialized = true;
    *EventSet = unique_event_set.id;

    return PAPI_OK;
}

int PAPI_add_events(int EventSet, int *Events, int number) {

    if (EventSet != unique_event_set.id) return PAPI_EINVAL;
    if (number <= 0) return PAPI_EINVAL;

    unique_event_set.num_events = number;

    return PAPI_OK;
}

int PAPI_cleanup_eventset(int EventSet) {

    if (EventSet != unique_event_set.id) return PAPI_EINVAL;
    if (!unique_event_set.initialized) return PAPI_EINVAL;

    return PAPI_OK;
}

int PAPI_destroy_eventset(int *EventSet) {

    if (EventSet == NULL) return PAPI_EINVAL;
    if (*EventSet == PAPI_NULL) return PAPI_EINVAL;
    if (!unique_event_set.initialized) return PAPI_EINVAL;
    if (*EventSet != unique_event_set.id) return PAPI_EINVAL;

    unique_event_set.initialized = false;
    unique_event_set.started = false;

    return PAPI_OK;
}

int PAPI_start(int EventSet) {

    if (EventSet != unique_event_set.id) return PAPI_EINVAL;
    if (!unique_event_set.initialized) return PAPI_EINVAL;

    unique_event_set.started = true;

    return PAPI_OK;
}

int PAPI_stop(int EventSet, long long * values) {

    if (EventSet != unique_event_set.id) return PAPI_EINVAL;
    if (!unique_event_set.initialized) return PAPI_EINVAL;

    PAPI_read(EventSet, values);
    unique_event_set.started = false;

    return PAPI_OK;
}

int PAPI_read(int EventSet, long long * values) {

    if (EventSet != unique_event_set.id) return PAPI_EINVAL;
    if (!unique_event_set.initialized) return PAPI_EINVAL;

    long long v = ++unique_event_set.times_called;
    if (values != NULL) {
        for (int i = 0 ; i < unique_event_set.num_events; ++i) {
            values[i] = v;
        }
    }

    return PAPI_OK;
}

int PAPI_get_opt(int option, void * ptr) {
    return PAPI_OK;
}

char *PAPI_strerror(int) {
    return "PAPI stub strerror";
}
