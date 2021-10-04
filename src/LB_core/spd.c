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

#include "LB_core/spd.h"

#include "support/debug.h"
#include "support/gtree.h"

#include <stdlib.h>
#include <string.h>
#include <pthread.h>

/* TLS global variable to store the spd pointer */
__thread subprocess_descriptor_t *thread_spd = NULL;

/* Global subprocess descriptor */
static subprocess_descriptor_t global_spd = { 0 };

/* GTree containing each subprocess descriptor */
typedef struct SPDInfo {
    const subprocess_descriptor_t *spd;
    pthread_t pthread;
} spd_info_t;
static GTree *spd_tree = NULL;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;


/*********************************************************************************/
/*    TLS global variable                                                        */
/*********************************************************************************/
void spd_enter_dlb(subprocess_descriptor_t *spd) {
    thread_spd = spd ? spd : &global_spd;
}

/*********************************************************************************/
/*    GTree modification functions                                               */
/*********************************************************************************/
static gint key_compare_func(gconstpointer a, gconstpointer b) {
    return *(const size_t*)a - *(const size_t*)b;
}

void spd_register(subprocess_descriptor_t *spd) {
    spd_info_t *spd_info = malloc(sizeof(spd_info_t));
    spd_info->spd = spd;
    spd_info->pthread = 0;

    pthread_mutex_lock(&mutex);
    {
        if (spd_tree == NULL) {
            spd_tree = g_tree_new_full(
                    (GCompareDataFunc)key_compare_func,
                    NULL, NULL, free);
        }
        g_tree_insert(spd_tree, spd, spd_info);
    }
    pthread_mutex_unlock(&mutex);
}

void spd_unregister(const subprocess_descriptor_t *spd) {
    pthread_mutex_lock(&mutex);
    {
        g_tree_remove(spd_tree, spd);
    }
    pthread_mutex_unlock(&mutex);
}

__attribute__((destructor))
static void spd_tree_dtor(void) {
    g_tree_destroy(spd_tree);
}

/*********************************************************************************/
/*    Setter and getter of the assigned pthread per spd                          */
/*********************************************************************************/
void spd_set_pthread(const subprocess_descriptor_t *spd, pthread_t pthread) {
    pthread_mutex_lock(&mutex);
    {
        spd_info_t *spd_info = g_tree_lookup(spd_tree, spd);
        spd_info->pthread = pthread;
    }
    pthread_mutex_unlock(&mutex);
}

pthread_t spd_get_pthread(const subprocess_descriptor_t *spd) {
    pthread_t pthread;
    pthread_mutex_lock(&mutex);
    {
        spd_info_t *spd_info = g_tree_lookup(spd_tree, spd);
        pthread = spd_info->pthread;
    }
    pthread_mutex_unlock(&mutex);
    return pthread;
}

/*********************************************************************************/
/*    Obtain a list of pointers (NULL terminated) of spds                        */
/*********************************************************************************/
static gint tree_to_list(gpointer key, gpointer value, gpointer data) {
    subprocess_descriptor_t *spd = key;
    subprocess_descriptor_t ***list_p = data;
    **list_p = spd;
    ++(*list_p);

    /* return false to not stop traversing */
    return false;
}

const subprocess_descriptor_t** spd_get_spds(void) {
    const subprocess_descriptor_t **spds;
    pthread_mutex_lock(&mutex);
    {
        spds = malloc(sizeof(subprocess_descriptor_t *) * (g_tree_nnodes(spd_tree)+1));
        const subprocess_descriptor_t **p = spds;
        g_tree_foreach(spd_tree, tree_to_list, &p);
        *p = NULL;
    }
    pthread_mutex_unlock(&mutex);

    return spds;
}
