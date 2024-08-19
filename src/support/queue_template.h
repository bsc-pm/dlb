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

/* Include this header multiple times to implement a fixed-size circular queue.
 * Before inclusion define at least QUEUE_T and QUEUE_SIZE to the type the
 * queue can hold.
 *
 * QUEUE_KEY_T can be defined to set the type of the primary key, which should
 * be either a subtype of QUEUE_T if using structs, or QUEUE_T.
 */

#ifndef QUEUE_TEMPLATE_H
#define QUEUE_TEMPLATE_H

#include "support/debug.h"

#include <stdlib.h>
#include <string.h>

#define QUEUE_IMPL(word) QUEUE_COMB1(QUEUE_PREFIX,word)
#define QUEUE_COMB1(pre, word) QUEUE_COMB2(pre, word)
#define QUEUE_COMB2(pre, word) pre##word

#endif /* QUEUE_TEMPLATE_H */

#ifndef QUEUE_T
#error "QUEUE_T must be defined"
#endif

#ifndef QUEUE_SIZE
#error "QUEUE_SIZE must be defined"
#endif

#ifndef QUEUE_KEY_T
#define QUEUE_KEY_T QUEUE_T
#endif

#define QUEUE_NAME QUEUE_COMB1(QUEUE_COMB1(queue,_), QUEUE_T)
#define QUEUE_PREFIX QUEUE_COMB1(QUEUE_NAME, _)

#define QUEUE_init    QUEUE_IMPL(init)
#define QUEUE_clear   QUEUE_IMPL(clear)
#define QUEUE_size    QUEUE_IMPL(size)
#define QUEUE_empty   QUEUE_IMPL(empty)
#define QUEUE_front   QUEUE_IMPL(front)
#define QUEUE_back    QUEUE_IMPL(back)
#define QUEUE_next    QUEUE_IMPL(next)
#define QUEUE_enqueue QUEUE_IMPL(enqueue)
#define QUEUE_dequeue QUEUE_IMPL(dequeue)
#define QUEUE_delete  QUEUE_IMPL(delete)
#define QUEUE_remove  QUEUE_IMPL(remove)
#define QUEUE_replace QUEUE_IMPL(replace)
#define QUEUE_search  QUEUE_IMPL(search)

/* Qeue template:
 * - items is actually QUEUE_SIZE + 1 because we don't save rear, but next.  FIXME
 *   (it simplifies some functions a little bit)
 * - 'front' points to the first element
 * - 'next' points to the next spot where to push
 */

typedef struct QUEUE_NAME {
    QUEUE_T items[QUEUE_SIZE];
    size_t front;                   /* position to the first element */
    size_t next;                    /* position to the last empty spot */
    size_t capacity;
} QUEUE_NAME;


/* Initializers */

static inline void QUEUE_init(QUEUE_NAME *queue) {
    *queue = (const QUEUE_NAME) {
        .capacity = QUEUE_SIZE,     /* mainly for debugging */
    };
}

static inline void QUEUE_clear(QUEUE_NAME *queue) {
    queue->front = 0;
    queue->next = 0;
}


/* Capacity */

static inline size_t QUEUE_size(const QUEUE_NAME *queue) {
    return queue->next >= queue->front
        ? queue->next - queue->front
        : QUEUE_SIZE - queue->front + queue->next;
}

static inline bool QUEUE_empty(const QUEUE_NAME *queue) {
    return queue->front == queue->next;
}


/* Element access */

static inline QUEUE_T* QUEUE_front(QUEUE_NAME *queue) {
    return QUEUE_empty(queue) ? NULL : &queue->items[queue->front];
}

static inline QUEUE_T* QUEUE_back(QUEUE_NAME *queue) {
    size_t rear = queue->next > 0 ? queue->next-1 : QUEUE_SIZE-1;
    return QUEUE_empty(queue) ? NULL : &queue->items[rear];
}

static inline QUEUE_T* QUEUE_next(QUEUE_NAME *queue, QUEUE_T *item) {
    if (QUEUE_empty(queue) ) {
        return NULL;
    }
    else if (queue->next > queue->front) {
        /* queue is linear */
        size_t next_index = item + 1 - queue->items;
        return next_index < queue->next ? item + 1 : NULL;
    }
    else {
        /* queue is circular */
        size_t next_index = item + 1 < queue->items + QUEUE_SIZE
            ? item + 1 - queue->items
            : 0;
        return next_index > queue->front || next_index < queue->next
            ? &queue->items[next_index] : NULL;
    }
}


/* Modifiers */

/* Enqueue item to the back of the queue */
static inline int QUEUE_enqueue(QUEUE_NAME *queue, QUEUE_T item) {
    size_t new_next = (queue->next + 1) % QUEUE_SIZE;
    if (unlikely(new_next == queue->front)) {
        return -1;
    }
    queue->items[queue->next] = item;
    queue->next = new_next;
    return 0;
}

/* Dequeue item from the front of the queue */
static inline int QUEUE_dequeue(QUEUE_NAME *queue, QUEUE_T *item) {
    if (QUEUE_empty(queue)) {
        return -1;
    }
    *item = queue->items[queue->front];
    queue->front = (queue->front + 1) % QUEUE_SIZE;
    return 0;
}

/* Delete element pointed by argument */
static inline void QUEUE_delete(QUEUE_NAME *queue, const QUEUE_T *item) {
    if (QUEUE_empty(queue)) {
        return;
    }

    if (item == &queue->items[queue->front]) {
        /* item is first element, just advance front */
        queue->front = (queue->front + 1) % QUEUE_SIZE;
        return;
    }

    size_t array_index = item - queue->items;

    // FIXME: more efficient?
    if (unlikely(
                /* queue is linear */
                (queue->next > queue->front
                 && (array_index < queue->front || array_index >= queue->next))
                ||
                /* queue is circular */
                (queue->next <= queue->front
                 && (array_index >= queue->next && array_index < queue->front))
                )) {
        /* out of bounds */
        return;
    }

    size_t write_index = array_index;
    for (size_t read_index = (array_index + 1) % QUEUE_SIZE;
            read_index != queue->next;
            read_index = (read_index + 1) % QUEUE_SIZE) {
        /* move items forward in the queue */
        queue->items[write_index] = queue->items[read_index];
        write_index = (write_index + 1) % QUEUE_SIZE;
    }
    queue->next = write_index;
}

/* Remove all items that match with key */
static inline void QUEUE_remove(QUEUE_NAME *queue, QUEUE_KEY_T key) {

    /* advance front for every first item that matches key */
    while (!QUEUE_empty(queue)
            && memcmp(&queue->items[queue->front], &key, sizeof(QUEUE_KEY_T)) == 0) {
        queue->front = (queue->front + 1) % QUEUE_SIZE;
    }

    if (QUEUE_empty(queue)) {
        return;
    }

    /* front does not match, iterate from 2nd item */
    size_t write_index = (queue->front + 1) % QUEUE_SIZE;
    for (size_t read_index = write_index;
            read_index != queue->next;
            read_index = (read_index + 1) % QUEUE_SIZE) {
        if (read_index != write_index) {
            /* move items forward in the queue */
            queue->items[write_index] = queue->items[read_index];
        }
        if (memcmp(&queue->items[read_index], &key, sizeof(QUEUE_KEY_T)) != 0) {
            /* write_index only advances when element is not to be removed */
            write_index = (write_index + 1) % QUEUE_SIZE;
        }
    }
    queue->next = write_index;
}

/* Replace the first item that matches by key */
static inline void QUEUE_replace(QUEUE_NAME *queue, QUEUE_T item) {
    for (size_t index = queue->front;
            index != queue->next;
            index = (index + 1) % QUEUE_SIZE) {
        if (memcmp(&queue->items[index], &item, sizeof(QUEUE_KEY_T)) == 0) {
            queue->items[index] = item;
            return;
        }
    }
}

/* Return reference to the first item that matches by key */
static inline QUEUE_T* QUEUE_search(QUEUE_NAME *queue, QUEUE_KEY_T key) {
    for (size_t index = queue->front;
            index != queue->next;
            index = (index + 1) % QUEUE_SIZE) {
        if (memcmp(&queue->items[index], &key, sizeof(QUEUE_KEY_T)) == 0) {
            return &queue->items[index];
        }
    }
    return NULL;
}

#undef QUEUE_T
#undef QUEUE_SIZE
#undef QUEUE_KEY_T
#undef QUEUE_PREFIX
#undef QUEUE_NAME
#undef QUEUE_init
#undef QUEUE_clear
#undef QUEUE_size
#undef QUEUE_empty
#undef QUEUE_front
#undef QUEUE_back
#undef QUEUE_next
#undef QUEUE_enqueue
#undef QUEUE_dequeue
#undef QUEUE_delete
#undef QUEUE_remove
#undef QUEUE_replace
#undef QUEUE_search
