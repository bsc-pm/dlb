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

/* Credits: https://www.davidpriver.com/ctemplates.html */

/* Include this header multiple times to implement a simplistic array.
 * Before inclusion define at least ARRAY_T to the type the array can hold.
 */

#ifndef ARRAY_TEMPLATE_H
#define ARRAY_TEMPLATE_H

#include "support/debug.h"

#include <stdlib.h>

#define ARRAY_IMPL(word) ARRAY_COMB1(ARRAY_PREFIX,word)
#define ARRAY_COMB1(pre, word) ARRAY_COMB2(pre, word)
#define ARRAY_COMB2(pre, word) pre##word

#endif /* ARRAY_TEMPLATE_H */

#ifndef ARRAY_T
#error "ARRAY_T must be defined"
#endif

#ifndef ARRAY_KEY_T
#define ARRAY_KEY_T ARRAY_T
#endif

#define ARRAY_NAME ARRAY_COMB1(ARRAY_COMB1(array,_), ARRAY_T)
#define ARRAY_PREFIX ARRAY_COMB1(ARRAY_NAME, _)

#define ARRAY_init    ARRAY_IMPL(init)
#define ARRAY_count   ARRAY_IMPL(count)
#define ARRAY_clear   ARRAY_IMPL(clear)
#define ARRAY_destroy ARRAY_IMPL(destroy)
#define ARRAY_get     ARRAY_IMPL(get)
#define ARRAY_push    ARRAY_IMPL(push)
#define ARRAY_compar  ARRAY_IMPL(compar)
#define ARRAY_sort    ARRAY_IMPL(sort)

typedef struct ARRAY_NAME {
    ARRAY_T *items;
    size_t count;
    size_t capacity;
} ARRAY_NAME;

static inline void ARRAY_init(ARRAY_NAME *array, size_t capacity) {
    *array = (const ARRAY_NAME) {
        .items = malloc(sizeof(ARRAY_T) * capacity),
        .count = 0,
        .capacity = capacity,
    };
}

static inline size_t ARRAY_count(const ARRAY_NAME *array) {
    return array->count;
}

static inline void ARRAY_clear(ARRAY_NAME *array) {
    array->count = 0;
}

static inline void ARRAY_destroy(ARRAY_NAME *array) {
    free(array->items);
    *array = (const ARRAY_NAME) {};
}

static inline ARRAY_T ARRAY_get(const ARRAY_NAME *array, size_t index) {
    ensure(index < array->count,
            "Please report bug.");
    return array->items[index];
}

static inline void ARRAY_push(ARRAY_NAME *array, ARRAY_T item) {
    ensure(array->count < array->capacity,
            "ARRAY_push does not support dynamic size. Please report bug.");
    array->items[array->count++] = item;
}

static inline int ARRAY_compar(const void *p1, const void* p2) {
    ARRAY_KEY_T item1 = *((ARRAY_KEY_T *)p1);
    ARRAY_KEY_T item2 = *((ARRAY_KEY_T *)p2);
    return item1 - item2;
}

static inline void ARRAY_sort(ARRAY_NAME *array) {
    qsort(array->items, array->count, sizeof(ARRAY_T), ARRAY_compar);
}

#undef ARRAY_T
#undef ARRAY_KEY_T
#undef ARRAY_PREFIX
#undef ARRAY_NAME
#undef ARRAY_init
#undef ARRAY_count
#undef ARRAY_clear
#undef ARRAY_destroy
#undef ARRAY_get
#undef ARRAY_push
#undef ARRAY_compar
#undef ARRAY_sort
