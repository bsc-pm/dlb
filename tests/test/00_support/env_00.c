/*********************************************************************************/
/*  Copyright 2018 Barcelona Supercomputing Center                               */
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
/*  along with DLB.  If not, see <http://www.gnu.org/licenses/>.                 */
/*********************************************************************************/

/*<testinfo>
    test_generator="gens/basic-generator"
</testinfo>*/

#include "support/env.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

int main(int argc, char **argv) {

    /* Empty environment */
    char **env = malloc(1*sizeof(char*));
    env[0] = NULL;

    // Set initial value
    add_to_environ("VAR1", "value1", &env, ENV_OVERWRITE_NEVER);
    assert( strcmp(env[0], "VAR1=value1") == 0 );
    assert( env[1] == NULL );
    // Do not Overwrite
    add_to_environ("VAR1", "value2", &env, ENV_OVERWRITE_NEVER);
    assert( strcmp(env[0], "VAR1=value1") == 0 );
    assert( env[1] == NULL );
    // Overwrite
    add_to_environ("VAR1", "value2", &env, ENV_OVERWRITE_ALWAYS);
    assert( strcmp(env[0], "VAR1=value2") == 0 );
    assert( env[1] == NULL );
    // Modify only if exists (it does)
    add_to_environ("VAR1", "value3", &env, ENV_UPDATE_IF_EXISTS);
    assert( strcmp(env[0], "VAR1=value3") == 0 );
    assert( env[1] == NULL );
    // Modify only if exists (it doesn't)
    add_to_environ("VAR2", "xxx", &env, ENV_UPDATE_IF_EXISTS);
    assert( strcmp(env[0], "VAR1=value3") == 0 );
    assert( env[1] == NULL );
    // Append
    add_to_environ("VAR1", "value4", &env, ENV_APPEND);
    assert( strcmp(env[0], "VAR1=value3 value4") == 0 );
    assert( env[1] == NULL );

    free(env[0]);
    free(env);

    return 0;
}
