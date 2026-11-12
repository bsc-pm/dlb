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

#include "seanerbus/c/seanerbus.h"

#include <stdlib.h>
#include <string.h>
#include <sys/eventfd.h>
#include <unistd.h>

/* stub struct representations, real types are in C++ */

typedef struct seanerbus_entity {
    int dummy;
} seanerbus_entity_t;

typedef struct seanerbus_message {
    uint64_t matching_id;
    uint64_t payload_type;
    size_t   payload_size;
    void*    payload;
} seanerbus_message_t;

typedef struct seanerbus_connection {
    int fd;
    int shutdown;

    seanerbus_message_callback_t request_cb;
    void* request_user_data;
    const seanerbus_message_t* message;

    seanerbus_resolve_callback_t resolve_cb;
    void* resolve_user_data;
} seanerbus_connection_t;


/* ----- Errors ----- */

const char* seanerbus_error_message(const seanerbus_error_t* error)
{
    return "error";
}

void seanerbus_error_destroy(seanerbus_error_t* error)
{
}

/* ----- Value helpers ----- */

seanerbus_byte_view_t seanerbus_byte_view_from_data(const void* data, size_t size)
{
    return (seanerbus_byte_view_t){
        .data = data,
        .size = size,
    };
}

seanerbus_name_entry_t seanerbus_name_entry_from_c_strings(const char* key, const char* value)
{
    return (seanerbus_name_entry_t){0};
}

/* ----- Entity ----- */

seanerbus_status_t seanerbus_entity_clone(
        const seanerbus_entity_t* entity,
        seanerbus_entity_t** out_entity,
        seanerbus_error_t** out_error)
{
    if (entity == NULL || out_entity == NULL) return SEANERBUS_STATUS_INVALID_ARGUMENT;

    seanerbus_entity_t *copy = malloc(sizeof(*copy));
    if (copy == NULL) return SEANERBUS_STATUS_ALLOCATION_FAILED;

    *copy = *entity;
    *out_entity = copy;
    if (out_error) *out_error = NULL;

    return SEANERBUS_STATUS_OK;
}

void seanerbus_entity_destroy(seanerbus_entity_t* entity)
{
    free(entity);
}

/* ----- Message ----- */

seanerbus_status_t seanerbus_message_create(
        const seanerbus_entity_t* entity,
        uint64_t payload_type,
        seanerbus_byte_view_t payload,
        uint64_t matching_id,
        seanerbus_message_t** out_message,
        seanerbus_error_t** out_error)
{
    if (entity == NULL || out_message == NULL) return SEANERBUS_STATUS_INVALID_ARGUMENT;

    (void)entity;

    seanerbus_message_t *msg = malloc(sizeof(*msg));
    if (msg == NULL) return SEANERBUS_STATUS_ALLOCATION_FAILED;

    void *new_payload = NULL;
    if (payload.size > 0) {
        new_payload = malloc(payload.size);
        if (new_payload == NULL) {
            free(msg);
            return SEANERBUS_STATUS_ALLOCATION_FAILED;
        }
        memcpy(new_payload, payload.data, payload.size);
    }

    *msg = (seanerbus_message_t){
        .matching_id = matching_id,
        .payload_type = payload_type,
        .payload_size = payload.size,
        .payload = new_payload,
    };
    *out_message = msg;
    if (out_error) *out_error = NULL;

    return SEANERBUS_STATUS_OK;
}

seanerbus_status_t seanerbus_message_get_payload(
        const seanerbus_message_t* message,
        seanerbus_byte_view_t* out_payload,
        seanerbus_error_t** out_error)
{
    if (message == NULL || out_payload == NULL) return SEANERBUS_STATUS_INVALID_ARGUMENT;

    *out_payload = seanerbus_byte_view_from_data(message->payload, message->payload_size);
    if (out_error) *out_error = NULL;

    return SEANERBUS_STATUS_OK;
}

seanerbus_status_t seanerbus_message_get_payload_type(
        const seanerbus_message_t* message,
        uint64_t* out_payload_type,
        seanerbus_error_t** out_error)
{
    if (message == NULL || out_payload_type == NULL) return SEANERBUS_STATUS_INVALID_ARGUMENT;

    *out_payload_type = message->payload_type;
    if (out_error) *out_error = NULL;

    return SEANERBUS_STATUS_OK;
}

void seanerbus_message_destroy(seanerbus_message_t* message)
{
    if (message == NULL) return;
    free(message->payload);
    free(message);
}

/* ----- Connection lifecycle ----- */

seanerbus_status_t seanerbus_connection_create(
        const char* pub_sub_endpoint,
        const char* req_res_endpoint,
        seanerbus_connection_t** out_connection,
        seanerbus_error_t** out_error)
{
    if (out_connection == NULL
            || pub_sub_endpoint == NULL
            || req_res_endpoint == NULL) {
        return SEANERBUS_STATUS_INVALID_ARGUMENT;
    }

    seanerbus_connection_t *conn = malloc(sizeof(*conn));
    if (conn == NULL) return SEANERBUS_STATUS_ALLOCATION_FAILED;

    /* Counter starts at 1 => permanently POLLIN, so poll() never waits. */
    conn->fd = eventfd(1, EFD_NONBLOCK | EFD_CLOEXEC);
    if (conn->fd < 0) {
        free(conn);
        return SEANERBUS_STATUS_ALLOCATION_FAILED;
    }

    *out_connection = conn;
    if (out_error) *out_error = NULL;

    return SEANERBUS_STATUS_OK;
}


seanerbus_status_t seanerbus_connection_shutdown(
        seanerbus_connection_t* connection,
        seanerbus_error_t** out_error)
{
    connection->shutdown = 1;
    if (out_error) *out_error = NULL;

    return SEANERBUS_STATUS_OK;
}

void seanerbus_connection_destroy(seanerbus_connection_t* connection)
{
    if (connection == NULL) return;
    if (connection->fd > 0) close(connection->fd);
    free(connection);
}

/* ----- Connection I/O ----- */

seanerbus_status_t seanerbus_connection_get_fd(
        const seanerbus_connection_t* connection,
        int* out_fd,
        seanerbus_error_t** out_error)
{
    if (connection == NULL || out_fd == NULL) return SEANERBUS_STATUS_INVALID_ARGUMENT;

    *out_fd = connection->fd;
    if (out_error) *out_error = NULL;

    return SEANERBUS_STATUS_OK;
}

seanerbus_status_t seanerbus_connection_request(
        seanerbus_connection_t* connection,
        const seanerbus_message_t* message,
        seanerbus_message_callback_t callback,
        void* user_data,
        seanerbus_error_t** out_error)
{
    if (connection == NULL || message == NULL) return SEANERBUS_STATUS_INVALID_ARGUMENT;

    connection->request_cb = callback;
    connection->request_user_data = user_data;
    connection->message = message;
    if (out_error) *out_error = NULL;

    return SEANERBUS_STATUS_OK;
}

seanerbus_status_t seanerbus_resolve(
        seanerbus_connection_t* connection,
        const seanerbus_name_entry_t* name_entries,
        size_t name_entry_count,
        seanerbus_resolve_callback_t callback,
        void* user_data,
        seanerbus_error_t** out_error)
{
    if (connection == NULL) return SEANERBUS_STATUS_INVALID_ARGUMENT;

    (void)name_entries;
    (void)name_entry_count;

    connection->resolve_cb = callback;
    connection->resolve_user_data = user_data;
    if (out_error) *out_error = NULL;

    return SEANERBUS_STATUS_OK;
}

seanerbus_status_t seanerbus_connection_handle(
        seanerbus_connection_t* connection,
        seanerbus_error_t** out_error)
{
    if (connection == NULL) return SEANERBUS_STATUS_INVALID_ARGUMENT;

    if (connection->request_cb) {
        seanerbus_message_callback_t cb = connection->request_cb;
        connection->request_cb = NULL;
        cb(connection, SEANERBUS_STATUS_OK, connection->message, NULL, connection->request_user_data);
    }

    if (connection->resolve_cb) {
        seanerbus_resolve_callback_t cb = connection->resolve_cb;
        connection->resolve_cb = NULL;
        cb(connection, SEANERBUS_STATUS_OK, NULL, 0, NULL, connection->resolve_user_data);
    }

    if (out_error) *out_error = NULL;

    return SEANERBUS_STATUS_OK;
}
