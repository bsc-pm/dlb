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

/* Credits: https://nullprogram.com/blog/2016/10/07/ */
/* Credits: https://www.davidpriver.com/ctemplates.html */

#ifndef SMALL_ARRAY_TEMPLATE_H
#define SMALL_ARRAY_TEMPLATE_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stddef.h>
#include <stdlib.h>

#ifndef SMALL_ARRAY_DEFAULT_BYTES
#define SMALL_ARRAY_DEFAULT_BYTES 1024
#endif

#endif /* SMALL_ARRAY_TEMPLATE_H */

/* Required before including:
 *   SMALL_ARRAY_TYPE  :   the element type  (e.g. int, my_struct, void *)
 * Optional:
 *   SMALL_ARRAY_TNAME :   token-safe name   (e.g. int, my_struct, void_ptr)
 */

#ifndef SMALL_ARRAY_TYPE
#error "SMALL_ARRAY_TYPE must be defined"
#endif

#ifndef SMALL_ARRAY_TNAME
#define SMALL_ARRAY_TNAME SMALL_ARRAY_TYPE
#endif

#define _SA_CAT2(a,b)  a##b
#define _SA_CAT(a,b)   _SA_CAT2(a,b)
#define _SA_T(suffix)  _SA_CAT(small_array_, _SA_CAT(SMALL_ARRAY_TNAME, suffix))

/* Capacity in elements, at least 1 so that buffer[] is never zero-length.
 * Defined per instantiation because it depends on the element type. */
#define _SA_CAP \
    (SMALL_ARRAY_DEFAULT_BYTES >= sizeof(SMALL_ARRAY_TYPE) \
        ? SMALL_ARRAY_DEFAULT_BYTES / sizeof(SMALL_ARRAY_TYPE) : 1)


typedef struct _SA_T() {
    size_t            size;
    SMALL_ARRAY_TYPE *values;
    SMALL_ARRAY_TYPE  buffer[_SA_CAP];
} _SA_T(_t);


/* Returns a pointer to n elements, or NULL if allocation failed (or n is
 * too large). On failure the array is left empty, so _free is still safe.
 * Elements are uninitialized. NOTE: values may point into buffer, so the
 * struct must not be copied or returned by value after _init. */

static inline SMALL_ARRAY_TYPE* _SA_T(_init)(_SA_T(_t) *array, size_t n) {

    if (n <= _SA_CAP) {
        array->values = array->buffer;
    } else if (n > SIZE_MAX / sizeof(SMALL_ARRAY_TYPE)) {
        array->values = NULL;  // sizeof * n would overflow
    } else {
        array->values = malloc(sizeof(SMALL_ARRAY_TYPE) * n);
    }

    array->size = array->values ? n : 0;

    return array->values;
}

static inline void _SA_T(_free)(_SA_T(_t) *array) {

    if (array->values != array->buffer) {
        free(array->values);
    }
    array->values = NULL;
    array->size = 0;
}

#undef _SA_CAP
#undef _SA_T
#undef _SA_CAT
#undef _SA_CAT2
#undef SMALL_ARRAY_TNAME
#undef SMALL_ARRAY_TYPE
