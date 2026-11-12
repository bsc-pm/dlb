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

// This file contains portions derived from SeanerBUS library.
// Original copyright:
// SPDX-FileCopyrightText: 2026 SEANERGYS Partners <https://seanergys.eu>
// SPDX-License-Identifier: MIT

#ifndef SEANERBUS_H
#define SEANERBUS_H

#include <stddef.h>
#include <stdint.h>

typedef enum seanerbus_status
{
    SEANERBUS_STATUS_OK = 0,
    SEANERBUS_STATUS_INVALID_ARGUMENT = 1,
    SEANERBUS_STATUS_ALLOCATION_FAILED = 2,
    SEANERBUS_STATUS_INVALID_STATE = 3,
    SEANERBUS_STATUS_BACKEND_ERROR = 4,
    SEANERBUS_STATUS_EXCEPTION = 5,
    SEANERBUS_STATUS_UNKNOWN_ERROR = 6,
} seanerbus_status_t;

typedef enum seanerbus_payload_type
{
    SEANERBUS_PAYLOAD_TIME_VALUE_V1 = 0,
    SEANERBUS_PAYLOAD_RESOLVE_REQ_V1 = 1,
    SEANERBUS_PAYLOAD_RESOLVE_RES_V1 = 2,
    SEANERBUS_PAYLOAD_HELLO_WORLD_REQ_V1 = 3,
    SEANERBUS_PAYLOAD_HELLO_WORLD_RES_V1 = 4,
    SEANERBUS_PAYLOAD_VECTOR_REQ_V1 = 5,
    SEANERBUS_PAYLOAD_VECTOR_RES_V1 = 6,
    SEANERBUS_PAYLOAD_REVERSE_RESOLVE_REQ_V1 = 7,
    SEANERBUS_PAYLOAD_REVERSE_RESOLVE_RES_V1 = 8,
    SEANERBUS_PAYLOAD_FLUX_LIFECYCLE_EVENT_V1 = 9,
    SEANERBUS_PAYLOAD_REGISTER_RESOLVE_REQ_V1 = 10,
    SEANERBUS_PAYLOAD_REGISTER_RESOLVE_RES_V1 = 11,
    SEANERBUS_PAYLOAD_UNREGISTER_RESOLVE_REQ_V1 = 12,
    SEANERBUS_PAYLOAD_UNREGISTER_RESOLVE_RES_V1 = 13,
    SEANERBUS_PAYLOAD_HISTORY_REQ_V1 = 14,
    SEANERBUS_PAYLOAD_HISTORY_RES_V1 = 15,
    SEANERBUS_PAYLOAD_FLUX_CLASSIFY_JOB_REQ_V1 = 16,
    SEANERBUS_PAYLOAD_FLUX_CLASSIFY_JOB_RES_V1 = 17,
    SEANERBUS_PAYLOAD_FLUX_LIFECYCLE_HEARTBEAT_V1 = 18,
    SEANERBUS_PAYLOAD_ERROR_RES_V1 = 19,
    SEANERBUS_PAYLOAD_GET_REQ_V1 = 20,
    SEANERBUS_PAYLOAD_GET_RES_V1 = 21,
    SEANERBUS_PAYLOAD_SET_REQ_V1 = 22,
} seanerbus_payload_type_t;

typedef struct seanerbus_byte_view
{
    const void* data;
    size_t size;
} seanerbus_byte_view_t;

typedef struct seanerbus_string_view
{
    const char* data;
    size_t size;
} seanerbus_string_view_t;

typedef struct seanerbus_name_entry
{
    seanerbus_string_view_t key;
    seanerbus_string_view_t value;
} seanerbus_name_entry_t;

typedef struct seanerbus_connection seanerbus_connection_t;
typedef struct seanerbus_entity seanerbus_entity_t;
typedef struct seanerbus_error seanerbus_error_t;
typedef struct seanerbus_message seanerbus_message_t;

typedef void (*seanerbus_message_callback_t)(
        seanerbus_connection_t* connection,
        seanerbus_status_t status,
        const seanerbus_message_t* message,
        const seanerbus_error_t* error,
        void* user_data);

typedef void (*seanerbus_resolve_callback_t)(
        seanerbus_connection_t* connection,
        seanerbus_status_t status,
        const seanerbus_entity_t* const* entities,
        size_t entity_count,
        const seanerbus_error_t* error,
        void* user_data);

const char* seanerbus_error_message(const seanerbus_error_t* error);

void seanerbus_error_destroy(seanerbus_error_t* error);

seanerbus_byte_view_t seanerbus_byte_view_from_data(
        const void* data,
        size_t size);

seanerbus_name_entry_t seanerbus_name_entry_from_c_strings(
        const char* key,
        const char* value);

seanerbus_status_t seanerbus_entity_clone(
        const seanerbus_entity_t* entity,
        seanerbus_entity_t** out_entity,
        seanerbus_error_t** out_error);

void seanerbus_entity_destroy(seanerbus_entity_t* entity);

seanerbus_status_t seanerbus_message_create(
        const seanerbus_entity_t* entity,
        uint64_t payload_type,
        seanerbus_byte_view_t payload,
        uint64_t matching_id,
        seanerbus_message_t** out_message,
        seanerbus_error_t** out_error);

seanerbus_status_t seanerbus_message_get_payload(
        const seanerbus_message_t* message,
        seanerbus_byte_view_t* out_payload,
        seanerbus_error_t** out_error);

seanerbus_status_t seanerbus_message_get_payload_type(
        const seanerbus_message_t* message,
        uint64_t* out_payload_type,
        seanerbus_error_t** out_error);

void seanerbus_message_destroy(seanerbus_message_t* message);

seanerbus_status_t seanerbus_connection_create(
        const char* pub_sub_endpoint,
        const char* req_res_endpoint,
        seanerbus_connection_t** out_connection,
        seanerbus_error_t** out_error);

seanerbus_status_t seanerbus_connection_shutdown(
        seanerbus_connection_t* connection,
        seanerbus_error_t** out_error);

void seanerbus_connection_destroy(seanerbus_connection_t* connection);

seanerbus_status_t seanerbus_connection_get_fd(
        const seanerbus_connection_t* connection,
        int* out_fd,
        seanerbus_error_t** out_error);

seanerbus_status_t seanerbus_connection_request(
        seanerbus_connection_t* connection,
        const seanerbus_message_t* message,
        seanerbus_message_callback_t callback,
        void* user_data,
        seanerbus_error_t** out_error);

seanerbus_status_t seanerbus_resolve(
        seanerbus_connection_t* connection,
        const seanerbus_name_entry_t* name_entries,
        size_t name_entry_count,
        seanerbus_resolve_callback_t callback,
        void* user_data,
        seanerbus_error_t** out_error);

seanerbus_status_t seanerbus_connection_handle(
        seanerbus_connection_t* connection,
        seanerbus_error_t** out_error);

#endif /* SEANERBUS_H */
