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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "support/dlb_common.h"
#include "talp/backend.h"
#include "talp/backends/backend_utils.h"

#include <seanerbus/c/seanerbus.h>

#include <poll.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

// Daemon's own SEA_LOCAL_ADDRESS
#define BUS_ADDRESS "demobus://localhost:5398"

// Startup timeout
#define TIMEOUT_MS 5000

static const core_api_t *dlb_core_api = NULL;

static bool seanergys_plugin_initialized = false;	// Set once init() fully succeeds
static bool seanergys_plugin_started = false;      	// Set once start() runs

static seanerbus_connection_t *seanergys_conn = NULL;	// SEANERBUS connection

enum { SEANERGYS_MAX_SOCKETS = 64 };			// Upper bound on physical sockets per node

// One energy UUID per physical socket
static seanerbus_entity_t *seanergys_cpu_entities[SEANERGYS_MAX_SOCKETS];
static size_t seanergys_cpu_entity_count = 0;

static bool seanergys_resolve_done = false;		// Set by seanergys_resolved() once the naming server answers

static uint64_t seanergys_cpu_last_good[SEANERGYS_MAX_SOCKETS];  // Last good value per socket in case of failure 

// True once a socket's GET answers or fails to send
static bool seanergys_cpu_answered[SEANERGYS_MAX_SOCKETS];
static size_t seanergys_cpu_answered_count = 0;

static bool seanergys_answer_done = false;		// Set once every cached socket has answered

static size_t seanergys_slot_index[SEANERGYS_MAX_SOCKETS];	 // udata for seanerbus_connection_request()

static pthread_mutex_t seanergys_collect_mutex = PTHREAD_MUTEX_INITIALIZER; // Serializes collect()

static void seanergys_resolved(seanerbus_connection_t *conn, seanerbus_status_t st,
    const seanerbus_entity_t *const *entities, size_t entities_count,
    const seanerbus_error_t *error, void *udata)
{
    size_t i;

    (void) conn;
    (void) udata;

    seanergys_resolve_done = true;

    // See if request failed
    if (st != SEANERBUS_STATUS_OK) {
        PLUGIN_ERROR("SEANERGYS energy backend: resolve failed: %s\n",
                     seanerbus_error_message(error));
        return;
    }

    // Naming server answered, but no registered labels found
    if (entities_count == 0) {
        PLUGIN_ERROR("SEANERGYS energy backend: no socket energy entities found\n");
        return;
    }

    // Set to upper limit due to upper bound already being high
    if (entities_count > SEANERGYS_MAX_SOCKETS) {
        entities_count = SEANERGYS_MAX_SOCKETS; 
    }

    for (i = 0; i < entities_count; ++i) {
        /* Entities are borrowed and disappear once this callback return,
         * so each match must be cloned to survive until collect() uses it */
        if (seanerbus_entity_clone(entities[i], &seanergys_cpu_entities[i], NULL) != SEANERBUS_STATUS_OK) {
            seanergys_cpu_entities[i] = NULL;
        }
    }
    seanergys_cpu_entity_count = entities_count;
}

static void seanergys_answered(seanerbus_connection_t *conn, seanerbus_status_t st,
    const seanerbus_message_t *response, const seanerbus_error_t *error, void *udata)
{
    // Recover which socket this reply belongs to
    size_t idx = *(size_t *) udata;
    seanerbus_byte_view_t payload = {0};
    uint64_t type = 0;

    (void) conn;

    // Mark done either way, so a failed request doesn't block wait_for() forever
    seanergys_cpu_answered[idx] = true;
    seanergys_cpu_answered_count++;
    if (seanergys_cpu_answered_count >= seanergys_cpu_entity_count) {
        seanergys_answer_done = true;
    }

    // Request itself failed; leave seanergys_cpu_last_good[idx] untouched
    if (st != SEANERBUS_STATUS_OK) {
        PLUGIN_WARNING("SEANERGYS energy backend: request failed for socket %zu: %s\n",
                       idx, seanerbus_error_message(error));
        return;
    }

    // Read the reply's type and bytes
    if (seanerbus_message_get_payload_type(response, &type, NULL) != SEANERBUS_STATUS_OK ||
        seanerbus_message_get_payload(response, &payload, NULL) != SEANERBUS_STATUS_OK) {
        PLUGIN_WARNING("SEANERGYS energy backend: unreadable response for socket %zu\n", idx);
        return;
    }

    // Make sure it's the reply we expect
    if (type != SEANERBUS_PAYLOAD_GET_RES_V1 || payload.size < sizeof(uint64_t)) {
        PLUGIN_WARNING("SEANERGYS energy backend: unexpected response for socket %zu\n", idx);
        return;
    }

    memcpy(&seanergys_cpu_last_good[idx], payload.data, sizeof(uint64_t));
}

static int seanergys_wait_for(seanerbus_connection_t *conn, bool *completion_flag, const char *operation_name)
{
    seanerbus_error_t *err = NULL;
    seanerbus_status_t st;
    int fd = -1;

    // Blocks until *completion_flag is set by a callback or TIMEOUT_MS passes
    while (!*completion_flag) {
        // Fetch the file descriptor again each loop
        if ((st = seanerbus_connection_get_fd(conn, &fd, &err)) != SEANERBUS_STATUS_OK) {
            PLUGIN_ERROR("SEANERGYS energy backend: connection_get_fd failed: %s\n",
                         seanerbus_error_message(err));
            seanerbus_error_destroy(err);
            return DLB_BACKEND_ERROR;
        }

        // Sleeps here instead of busy-looping; wakes up once the broker sends something
        struct pollfd pfd = { fd, POLLIN, 0 };
        int rc = poll(&pfd, 1, TIMEOUT_MS);

        // Check if poll itself failed
        if (rc < 0) {
            PLUGIN_ERROR("SEANERGYS energy backend: poll failed while waiting for %s\n", operation_name);
            return DLB_BACKEND_ERROR;
        }
        // Check if we timed out instead of getting an answer
        if (rc == 0) {
            PLUGIN_ERROR("SEANERGYS energy backend: timeout waiting for %s\n", operation_name);
            return DLB_BACKEND_ERROR;
        }
        // Check if the connection itself died
        if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
            PLUGIN_ERROR("SEANERGYS energy backend: connection to the broker lost\n");
            return DLB_BACKEND_ERROR;
        }

        // Only here does seanergys_resolved() actually run
        if ((st = seanerbus_connection_handle(conn, &err)) != SEANERBUS_STATUS_OK) {
            PLUGIN_ERROR("SEANERGYS energy backend: connection_handle failed: %s\n",
                         seanerbus_error_message(err));
            seanerbus_error_destroy(err);
            return DLB_BACKEND_ERROR;
        }
    }
    
    return DLB_BACKEND_SUCCESS;
}

/*********************************************************************************/
/* Plugin functions                                                              */
/*********************************************************************************/

static int seanergys_backend_probe(void)
{
    seanerbus_connection_t *conn = NULL;
    seanerbus_error_t *err = NULL;

    // Confirm if the broker is actually reachable 
    if (seanerbus_connection_create(BUS_ADDRESS, BUS_ADDRESS, &conn, &err) != SEANERBUS_STATUS_OK) {
        seanerbus_error_destroy(err);
        return DLB_BACKEND_ERROR;
    }

    seanerbus_connection_shutdown(conn, NULL);
    seanerbus_connection_destroy(conn);
    return DLB_BACKEND_SUCCESS;
}

static int seanergys_backend_init(const core_api_t *core_api)
{
    seanerbus_error_t *err = NULL;
    seanerbus_status_t st;
    size_t i;

    if (seanergys_plugin_initialized) return DLB_BACKEND_ERROR;  // guard against double-init

    dlb_core_api = core_api;
    if (dlb_core_api->abi_version != DLB_BACKEND_ABI_VERSION ||
        dlb_core_api->struct_size != sizeof(core_api_t)) {
        return DLB_BACKEND_ERROR;
    }

    // Connect to the broker
    if ((st = seanerbus_connection_create(BUS_ADDRESS, BUS_ADDRESS,
            &seanergys_conn, &err)) != SEANERBUS_STATUS_OK) {
        PLUGIN_ERROR("SEANERGYS energy backend: connection_create failed: %s\n",
                     seanerbus_error_message(err));
        seanerbus_error_destroy(err);
        return DLB_BACKEND_ERROR;
    }

    // Ask the naming server for every socket's energy UUID
    seanerbus_name_entry_t names[4] = {
        seanerbus_name_entry_from_c_strings("domain",    "Metric"),
        seanerbus_name_entry_from_c_strings("metric",    "Energy"),
        seanerbus_name_entry_from_c_strings("device",    "Socket"),
        seanerbus_name_entry_from_c_strings("device_no", "*"),
    };

    // Queue the request
    if ((st = seanerbus_resolve(seanergys_conn, names, 4, seanergys_resolved, NULL, &err)) !=
         SEANERBUS_STATUS_OK) {
        PLUGIN_ERROR("SEANERGYS energy backend: resolve failed: %s\n",
                     seanerbus_error_message(err));
        seanerbus_error_destroy(err);
        return DLB_BACKEND_ERROR;
    }

    // Blocks here until seanergys_resolved() has run or times out
    if (seanergys_wait_for(seanergys_conn, &seanergys_resolve_done, "Socket energy resolution") !=
        DLB_BACKEND_SUCCESS) {
        return DLB_BACKEND_ERROR;
    }

    // Resolve succeeded but no matching energy UUIDs
    if (seanergys_cpu_entity_count == 0) {
        return DLB_BACKEND_ERROR;
    }

    /* Fills seanergys_slot_index[] so collect()'s answer callback can map
     * a reply back to which socket it belongs to */
    for (i = 0; i < seanergys_cpu_entity_count; ++i) {
        seanergys_slot_index[i] = i;
    }

    seanergys_plugin_initialized = true;

    return DLB_BACKEND_SUCCESS;
}

static int seanergys_backend_start(void)
{
    if (!seanergys_plugin_initialized) return DLB_BACKEND_ERROR;
    if (seanergys_plugin_started) return DLB_BACKEND_SUCCESS;  // already started, not an error

    seanergys_plugin_started = true;

    PLUGIN_PRINT("SEANERGYS energy backend started\n");

    return DLB_BACKEND_SUCCESS;
}

static int seanergys_backend_energy_collect(energy_readings_t *out)
{
    seanerbus_error_t *err = NULL;
    seanerbus_status_t st;
    size_t i;
    int result = DLB_BACKEND_SUCCESS;
    uint64_t total_joules = 0;

    if (out == NULL) return DLB_BACKEND_ERROR;
    if (!seanergys_plugin_started) return DLB_BACKEND_ERROR;

    // Only one collect()/finalize() touching seanergys_conn and the shared arrays at a time
    pthread_mutex_lock(&seanergys_collect_mutex);

    // Reset this call's tracking
    seanergys_answer_done = false;
    seanergys_cpu_answered_count = 0;
    for (i = 0; i < seanergys_cpu_entity_count; ++i) {
        seanergys_cpu_answered[i] = false;
    }

    // Send a GET to every cached socket; all sent before waiting for any reply
    for (i = 0; i < seanergys_cpu_entity_count; ++i) {
        seanerbus_message_t *request = NULL;

        // See if this socket was never resolved in the first place
        if (seanergys_cpu_entities[i] == NULL) {
            // Never resolved; count as answered so wait_for() won't block on it
            seanergys_cpu_answered[i] = true;
            seanergys_cpu_answered_count++;

            continue;
        }

        // See if building the request itself failed
        if ((st = seanerbus_message_create(seanergys_cpu_entities[i], SEANERBUS_PAYLOAD_GET_REQ_V1,
                seanerbus_byte_view_from_data(NULL, 0), 0, &request, &err)) != SEANERBUS_STATUS_OK) {
            PLUGIN_WARNING("SEANERGYS energy backend: message_create failed for socket %zu: %s\n",
                           i, seanerbus_error_message(err));
            seanerbus_error_destroy(err);

            // Couldn't even build the request; count as answered so wait_for() won't block on it
            seanergys_cpu_answered[i] = true;
            seanergys_cpu_answered_count++;

            continue;
        }

        // Send the request
        st = seanerbus_connection_request(seanergys_conn, request, seanergys_answered,
                &seanergys_slot_index[i], &err);
        seanerbus_message_destroy(request);  // Queued already; our copy isn't needed anymore

        // See if the send itself failed
        if (st != SEANERBUS_STATUS_OK) {
            PLUGIN_WARNING("SEANERGYS energy backend: connection_request failed for socket %zu: %s\n",
                           i, seanerbus_error_message(err));
            seanerbus_error_destroy(err);

            // Never actually sent; count as answered so wait_for() won't block on it
            seanergys_cpu_answered[i] = true;
            seanergys_cpu_answered_count++;
        }
    }

    // Every send already failed; nothing left to wait for
    if (seanergys_cpu_answered_count >= seanergys_cpu_entity_count) {
        seanergys_answer_done = true;
    }

    // See if waiting for every socket to answer failed (timeout or disconnect)
    if (seanergys_wait_for(seanergys_conn, &seanergys_answer_done,
            "Socket energy answers") != DLB_BACKEND_SUCCESS) {
        result = DLB_BACKEND_ERROR;
    } else {
        // Sum across all sockets to get the node's total CPU energy
        for (i = 0; i < seanergys_cpu_entity_count; ++i) {
            total_joules += seanergys_cpu_last_good[i];
        }

        // Convert J -> nj
        out->cpu  = (int64_t) (total_joules * 1000000000ULL);
        out->dram = 0;   // Not measured by this backend
        out->node = 0;   // Not measured by this backend
        out->gpu  = 0;   // Not measured by this backend
    }

    // Done touching the shared state
    pthread_mutex_unlock(&seanergys_collect_mutex);

    return result;
}

static int seanergys_backend_stop(void)
{
    seanergys_plugin_started = false;
    return DLB_BACKEND_SUCCESS;
}

static int seanergys_backend_finalize(void)
{
    size_t i;

    if (!seanergys_plugin_initialized) return DLB_BACKEND_SUCCESS;

    pthread_mutex_lock(&seanergys_collect_mutex);

    // Free every cached entity before the connection itself goes away
    for (i = 0; i < seanergys_cpu_entity_count; ++i) {
        seanerbus_entity_destroy(seanergys_cpu_entities[i]);
        seanergys_cpu_entities[i] = NULL;
    }
    seanergys_cpu_entity_count = 0;

    // shutdown() cancels anything still pending before destroy() frees the connection
    seanerbus_connection_shutdown(seanergys_conn, NULL);
    seanerbus_connection_destroy(seanergys_conn);
    seanergys_conn = NULL;

    seanergys_plugin_initialized = false;

    pthread_mutex_unlock(&seanergys_collect_mutex);

    return DLB_BACKEND_SUCCESS;
}

DLB_EXPORT_SYMBOL
backend_api_t* DLB_Get_Backend_API(void)
{
    static backend_api_t api = {
        .abi_version = DLB_BACKEND_ABI_VERSION,
        .struct_size = sizeof(backend_api_t),
        .name = "seanergys",
        .capabilities = {
            .energy = true,
        },
        .probe = seanergys_backend_probe,
        .init = seanergys_backend_init,
        .start = seanergys_backend_start,
        .stop = seanergys_backend_stop,
        .finalize = seanergys_backend_finalize,
        .energy = {
            .collect = seanergys_backend_energy_collect,
        },
    };
    return &api;
}
