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

#if NCPUS_AT_CONFIGURE_TIME > 1 && NCPUS_AT_CONFIGURE_TIME < 1025
enum { SMALL_ARRAY_DEFAULT_SIZE = NCPUS_AT_CONFIGURE_TIME };
#else
enum { SMALL_ARRAY_DEFAULT_SIZE = 64 };
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

typedef struct _SA_T() {
    size_t            size;
    SMALL_ARRAY_TYPE *values;
    SMALL_ARRAY_TYPE  buffer[SMALL_ARRAY_DEFAULT_SIZE];
} _SA_T(_t);

static inline SMALL_ARRAY_TYPE* _SA_T(_init)(_SA_T(_t) *array, size_t n) {

    array->size = n;
    array->values = (n <= SMALL_ARRAY_DEFAULT_SIZE)
              ? array->buffer
              : malloc(sizeof(SMALL_ARRAY_TYPE) * n);
    return array->values;
}

static inline void _SA_T(_free)(_SA_T(_t) *array) {

    if (array->size > SMALL_ARRAY_DEFAULT_SIZE) {
        free(array->values);
    }
    array->values = NULL;
}

#undef _SA_T
#undef _SA_CAT
#undef _SA_CAT2
#undef SMALL_ARRAY_TNAME
#undef SMALL_ARRAY_TYPE
