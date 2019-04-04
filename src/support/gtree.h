/*********************************************************************************/
/*  Copyright 2009-2019 Barcelona Supercomputing Center                          */
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

/* GTree implementtion from GLIB: */

/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GLib Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/.
 */

#ifndef GTREE_H
#define GTREE_H

#include <inttypes.h>
typedef int            gint;
typedef gint           gboolean;
typedef unsigned int   guint;
typedef int8_t         gint8;
typedef uint8_t        guint8;
typedef void*          gpointer;
typedef const void    *gconstpointer;
typedef gint         (*GCompareFunc) (gconstpointer a, gconstpointer b);
typedef gint         (*GCompareDataFunc) (gconstpointer a, gconstpointer b, gpointer user_data);
typedef void         (*GDestroyNotify) (gpointer data);
typedef gboolean     (*GTraverseFunc) (gpointer key, gpointer value, gpointer data);

typedef enum
{
    G_IN_ORDER,
    G_PRE_ORDER,
    G_POST_ORDER,
    G_LEVEL_ORDER
} GTraverseType;

typedef struct _GTree  GTree;


/* Balanced binary trees
 */
GTree*   g_tree_new             (GCompareFunc      key_compare_func);
GTree*   g_tree_new_with_data   (GCompareDataFunc  key_compare_func,
                                 gpointer          key_compare_data);
GTree*   g_tree_new_full        (GCompareDataFunc  key_compare_func,
                                 gpointer          key_compare_data,
                                 GDestroyNotify    key_destroy_func,
                                 GDestroyNotify    value_destroy_func);
GTree*   g_tree_ref             (GTree            *tree);
void     g_tree_unref           (GTree            *tree);
void     g_tree_destroy         (GTree            *tree);
void     g_tree_insert          (GTree            *tree,
                                 gpointer          key,
                                 gpointer          value);
void     g_tree_replace         (GTree            *tree,
                                 gpointer          key,
                                 gpointer          value);
gboolean g_tree_remove          (GTree            *tree,
                                 gconstpointer     key);
gboolean g_tree_steal           (GTree            *tree,
                                 gconstpointer     key);
gpointer g_tree_lookup          (GTree            *tree,
                                 gconstpointer     key);
gboolean g_tree_lookup_extended (GTree            *tree,
                                 gconstpointer     lookup_key,
                                 gpointer         *orig_key,
                                 gpointer         *value);
void     g_tree_foreach         (GTree            *tree,
                                 GTraverseFunc     func,
                                 gpointer          user_data);
void     g_tree_traverse        (GTree            *tree,
                                 GTraverseFunc     traverse_func,
                                 GTraverseType     traverse_type,
                                 gpointer          user_data);
gpointer g_tree_search          (GTree            *tree,
                                 GCompareFunc      search_func,
                                 gconstpointer     user_data);
gint     g_tree_height          (GTree            *tree);
gint     g_tree_nnodes          (GTree            *tree);


#endif /* GTREE_H */
