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

/* Subset of GSList from GLIB to implement a stack: */

/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
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

/*
 * MT safe
 */

#include "gslist.h"

#include <stdlib.h>


/**
 * g_slist_free:
 * @list: the first link of a #GSList
 *
 * DLB: free all elements in list
 **/
void
g_slist_free (GSList *list)
{
  while (list)
    {
      GSList *next = list->next;
      free(list);
      list = next;
    }
}

/**
 * g_slist_prepend:
 * @list: a #GSList
 * @data: the data for the new element
 *
 * Adds a new element on to the start of the list.
 *
 * The return value is the new start of the list, which
 * may have changed, so make sure you store the new value.
 *
 * |[<!-- language="C" -->
 * // Notice that it is initialized to the empty list.
 * GSList *list = NULL;
 * list = g_slist_prepend (list, "last");
 * list = g_slist_prepend (list, "first");
 * ]|
 *
 * Returns: the new start of the #GSList
 */
GSList*
g_slist_prepend (GSList   *list,
                 gpointer  data)
{
  GSList *new_list;

  new_list = malloc(sizeof(GSList));
  new_list->data = data;
  new_list->next = list;

  return new_list;
}

/**
 * g_slist_remove:
 * @list: a #GSList
 * @data: the data of the element to remove
 *
 * Removes an element from a #GSList.
 * If two elements contain the same data, only the first is removed.
 * If none of the elements contain the data, the #GSList is unchanged.
 *
 * Returns: the new start of the #GSList
 */
GSList*
g_slist_remove (GSList        *list,
                gconstpointer  data)
{
  GSList *tmp = NULL;
  GSList **previous_ptr = &list;

  while (*previous_ptr)
    {
      tmp = *previous_ptr;
      if (tmp->data == data)
        {
          *previous_ptr = tmp->next;
          free(tmp);
          break;
        }
      else
        {
          previous_ptr = &tmp->next;
        }
    }

  return list;
}

/**
 * g_slist_reverse:
 * @list: a #GSList
 *
 * Reverses a #GSList.
 *
 * Returns: the start of the reversed #GSList
 */
GSList*
g_slist_reverse (GSList *list)
{
  GSList *prev = NULL;

  while (list)
    {
      GSList *next = list->next;

      list->next = prev;

      prev = list;
      list = next;
    }

  return prev;
}

/**
 * g_slist_length:
 * @list: a #GSList
 *
 * Gets the number of elements in a #GSList.
 *
 * This function iterates over the whole list to
 * count its elements. To check whether the list is non-empty, it is faster to
 * check @list against %NULL.
 *
 * Returns: the number of elements in the #GSList
 */
guint
g_slist_length (GSList *list)
{
  guint length;

  length = 0;
  while (list)
    {
      length++;
      list = list->next;
    }

  return length;
}

/**
 * g_slist_foreach:
 * @list: a #GSList
 * @func: (scope call): the function to call with each element's data
 * @user_data: user data to pass to the function
 *
 * Calls a function for each element of a #GSList.
 *
 * It is safe for @func to remove the element from @list, but it must
 * not modify any part of the list after that element.
 */
void
g_slist_foreach (GSList   *list,
                 GFunc     func,
                 gpointer  user_data)
{
  while (list)
    {
      GSList *next = list->next;
      (*func) (list->data, user_data);
      list = next;
    }
}
