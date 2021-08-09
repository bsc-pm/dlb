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
    dlb_setenv("VAR1", "value1", &env, ENV_OVERWRITE_NEVER);
    assert( strcmp(env[0], "VAR1=value1") == 0 );
    assert( env[1] == NULL );
    // Do not Overwrite
    dlb_setenv("VAR1", "value2", &env, ENV_OVERWRITE_NEVER);
    assert( strcmp(env[0], "VAR1=value1") == 0 );
    assert( env[1] == NULL );
    // Overwrite
    dlb_setenv("VAR1", "value2", &env, ENV_OVERWRITE_ALWAYS);
    assert( strcmp(env[0], "VAR1=value2") == 0 );
    assert( env[1] == NULL );
    // Modify only if exists (it does)
    dlb_setenv("VAR1", "value3", &env, ENV_UPDATE_IF_EXISTS);
    assert( strcmp(env[0], "VAR1=value3") == 0 );
    assert( env[1] == NULL );
    // Modify only if exists (it doesn't)
    dlb_setenv("VAR2", "xxx", &env, ENV_UPDATE_IF_EXISTS);
    assert( strcmp(env[0], "VAR1=value3") == 0 );
    assert( env[1] == NULL );
    // Append
    dlb_setenv("VAR1", "value4", &env, ENV_APPEND);
    assert( strcmp(env[0], "VAR1=value3 value4") == 0 );
    assert( env[1] == NULL );

    free(env[0]);
    free(env);

    /* Current environment */

    const char *var_name = "DLB_TESTING_VAR";

    // Set initial value
    dlb_setenv(var_name, "value1", NULL, ENV_OVERWRITE_NEVER);
    assert( strcmp(getenv(var_name), "value1") == 0 );
    // Do not Overwrite
    dlb_setenv(var_name, "value2", NULL, ENV_OVERWRITE_NEVER);
    assert( strcmp(getenv(var_name), "value1") == 0 );
    // Overwrite
    dlb_setenv(var_name, "value2", NULL, ENV_OVERWRITE_ALWAYS);
    assert( strcmp(getenv(var_name), "value2") == 0 );
    // Modify only if exists (it does)
    dlb_setenv(var_name, "value3", NULL, ENV_UPDATE_IF_EXISTS);
    assert( strcmp(getenv(var_name), "value3") == 0 );
    // Append
    dlb_setenv(var_name, "value4", NULL, ENV_APPEND);
    assert( strcmp(getenv(var_name), "value3 value4") == 0 );
    // Modify only if exists (it doesn't)
    unsetenv(var_name);
    dlb_setenv(var_name, "xxx", NULL, ENV_UPDATE_IF_EXISTS);
    assert( getenv(var_name) == NULL );

    return 0;
}
