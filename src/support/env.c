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

#include "support/env.h"

#include "support/debug.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Implementation based on glibc's setenv:
 *  https://sourceware.org/git/?p=glibc.git;a=blob;f=stdlib/setenv.c
 */
static void add_to_environ(const char *name, const char *value, char ***next_environ,
        env_add_condition_t condition) {
    const size_t namelen = strlen (name);
    size_t size = 0;
    char **ep;

    for (ep=*next_environ; *ep != NULL; ++ep) {
        if (!strncmp(*ep, name, namelen) && (*ep)[namelen] == '=')
            break;
        else
            ++size;
    }

    if (*ep == NULL) {
        /* Variable does not exist */
        if (condition == ENV_UPDATE_IF_EXISTS) return;

        /* Allocate new variable */
        // realloc (num_existing_vars + 1(new_var) + 1(NULL))
        char **new_environ = realloc(*next_environ, (size + 2)*sizeof(char*));
        fatal_cond(!new_environ, "realloc failed");
        *next_environ = new_environ;
        // set last position of next_environ
        new_environ[size+1] = NULL;
        // pointer where new variable will be
        ep = new_environ + size;

    } else {
        /* Variable does exist */
        if (condition == ENV_OVERWRITE_NEVER) return;

        // ep already points to the variable we will overwrite

        /* Only if the variable exists and needs appending, allocate new buffer */
        if (condition == ENV_APPEND) {
            const size_t origlen = strlen(*ep);
            const size_t newlen = origlen + 1 + strlen(value) +1;
            char *new_np = realloc(*ep, newlen);
            fatal_cond(!new_np, "realloc failed");
            *ep = new_np;
            sprintf(new_np+origlen, " %s", value);
            return;
        }
    }

    const size_t varlen = strlen(name) + 1 + strlen(value) + 1;
    char *np = malloc(varlen);
    snprintf(np, varlen, "%s=%s", name, value);
    *ep = np;
}

/* Modify environment:
 *  - name: environment variable name
 *  - value: environment variable value
 *  - next_environ: environ pointer, or NULL to modify current environment
 *  - condition: overwrite/append conditions
 */
void dlb_setenv(const char *name, const char *value, char ***next_environ,
        env_add_condition_t condition) {
    if (next_environ == NULL) {
        /* Modify current environment */
        const char *current_value = getenv(name);
        if (!current_value) {
            if (condition == ENV_UPDATE_IF_EXISTS) return;
            if (condition == ENV_APPEND) condition = ENV_OVERWRITE_NEVER;
        }
        size_t len;
        char *new_value;
        switch(condition) {
            case ENV_OVERWRITE_NEVER:
                setenv(name, value, 0);
                break;
            case ENV_UPDATE_IF_EXISTS:
                DLB_FALLTHROUGH;
            case ENV_OVERWRITE_ALWAYS:
                setenv(name, value, 1);
                break;
            case ENV_APPEND:
                /* We can assume current_value is not NULL */
                len = strlen(current_value) + 1 + strlen(value) + 1;
                new_value = malloc(sizeof(char)*len);
                sprintf(new_value, "%s %s", current_value, value);
                setenv(name, new_value, 1);
                free(new_value);
                break;
        }
    } else {
        /* Modify next_environ */
        add_to_environ(name, value, next_environ, condition);
    }
}
