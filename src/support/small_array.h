/*********************************************************************************/
/*  Copyright 2009-2021 Barcelona Supercomputing Center                          */
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

/* Credits: https://nullprogram.com/blog/2016/10/07/ */

#ifndef SMALL_ARRAY_H
#define SMALL_ARRAY_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "support/queues.h"

#include <sys/types.h>
#include <stdlib.h>

#if NCPUS_AT_CONFIGURE_TIME > 1 && NCPUS_AT_CONFIGURE_TIME < 1025
enum { SMALL_ARRAY_DEFAULT_SIZE = NCPUS_AT_CONFIGURE_TIME };
#else
enum { SMALL_ARRAY_DEFAULT_SIZE = 64 };
#endif

typedef struct SmallArrayInt {
    size_t size;
    int *values;
    int buffer[SMALL_ARRAY_DEFAULT_SIZE];
} small_array_int_t;
typedef small_array_int_t small_array_int;

static inline int* small_array_int_init(small_array_int_t *array, size_t size) {
    array->size = size;
    if (size <= SMALL_ARRAY_DEFAULT_SIZE) {
        array->values = array->buffer;
    } else {
        array->values = malloc(sizeof(int) * size);
    }
    return array->values;
}

static inline void small_array_int_free(small_array_int_t *array) {
    if (array->size > SMALL_ARRAY_DEFAULT_SIZE) {
        free(array->values);
    }
    array->values = NULL;
}


typedef struct SmallArrayPID {
    size_t size;
    pid_t *values;
    pid_t buffer[SMALL_ARRAY_DEFAULT_SIZE];
} small_array_pid_t;

static inline pid_t* small_array_pid_t_init(small_array_pid_t *array, size_t size) {
    array->size = size;
    if (size <= SMALL_ARRAY_DEFAULT_SIZE) {
        array->values = array->buffer;
    } else {
        array->values = malloc(sizeof(pid_t) * size);
    }
    return array->values;
}

static inline void small_array_pid_t_free(small_array_pid_t *array) {
    if (array->size > SMALL_ARRAY_DEFAULT_SIZE) {
        free(array->values);
    }
    array->values = NULL;
}


typedef struct SmallArrayLeWIRequest {
    size_t size;
    lewi_request_t *values;
    lewi_request_t buffer[SMALL_ARRAY_DEFAULT_SIZE];
} small_array_lewi_request_t;

static inline lewi_request_t* small_array_lewi_request_t_init(small_array_lewi_request_t *array, size_t size) {
    array->size = size;
    if (size <= SMALL_ARRAY_DEFAULT_SIZE) {
        array->values = array->buffer;
    } else {
        array->values = malloc(sizeof(lewi_request_t) * size);
    }
    return array->values;
}

static inline void small_array_lewi_request_t_free(small_array_lewi_request_t *array) {
    if (array->size > SMALL_ARRAY_DEFAULT_SIZE) {
        free(array->values);
    }
    array->values = NULL;
}

#define SMALL_ARRAY(type, var_name, size) \
    small_array_##type __##var_name __attribute__ ((__cleanup__(small_array_##type##_free))); \
    type *var_name = small_array_##type##_init(&__##var_name, size);


#endif /* SMALL_ARRAY_H */
