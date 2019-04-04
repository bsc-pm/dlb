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

/*<testinfo>
    test_generator="gens/basic-generator"
</testinfo>*/

#include "support/gtree.h"

#include <stdbool.h>
#include <assert.h>

static gint key_compare_func(gconstpointer a, gconstpointer b) {
    return *(const char*)a - *(const char*)b;
}

static gint find_A(gpointer key, gpointer value, gpointer data) {
    char *ch = key;
    return *ch == 'A';
}


int main(int argc, char **argv) {
    GTree *tree;
    char chars[62];
    guint i, j;

    tree = g_tree_new(key_compare_func);

    // Insert [0-9][A-Z][a-z]
    i = 0;
    for (j=0; j<10; ++j, ++i)
    {
        chars[i] = '0' + j;
        g_tree_insert(tree, &chars[i], &chars[i]);
    }
    for (j=0; j<26; ++j, ++i)
    {
        chars[i] = 'a' + j;
        g_tree_insert(tree, &chars[i], &chars[i]);
    }
    for (j=0; j<26; ++j, ++i)
    {
        chars[i] = 'A' + j;
        g_tree_insert(tree, &chars[i], &chars[i]);
    }

    // Lookup each element
    for (i=0; i<10+26+26; ++i)
    {
        assert( g_tree_lookup(tree, &chars[i]) != NULL );
    }

    // Check sizes
    assert( g_tree_nnodes(tree) == 10 + 26 + 26 );
    assert( g_tree_height(tree) == 7 );

    g_tree_foreach(tree, find_A, NULL);

    // Remove [0-9]
    for (i=0; i<10; ++i) {
        g_tree_remove(tree, &chars[i]);
    }

    // Check sizes
    assert( g_tree_nnodes(tree) == 26 + 26 );
    assert( g_tree_height(tree) == 7 );

    g_tree_ref(tree);
    g_tree_unref(tree);
    assert( tree != NULL );
    g_tree_destroy(tree);

    return 0;
}
