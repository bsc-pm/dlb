/*********************************************************************************/
/*  Copyright 2017 Barcelona Supercomputing Center                               */
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

#include "assert_noshm.h"

#include <apis/dlb_drom.h>

#include <sched.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>

/* Basic DROM init / pre / post / finalize and child checking the environment  */

void __gcov_flush() __attribute__((weak));

static void test_fork(const cpu_set_t *mask, char ***env) {
    pid_t pid = fork();
    assert( pid >= 0 );
    if (pid == 0) {
        pid = getpid();
        assert( DLB_DROM_Attach()                                   == DLB_SUCCESS );
        assert( DLB_DROM_PreInit(pid, mask, DLB_STEAL_CPUS, env)    == DLB_SUCCESS );
        assert( DLB_DROM_Detach()                                   == DLB_SUCCESS );

        if (__gcov_flush) __gcov_flush();

        // Exec into a shell process to check if the environment is correctly modified
        const char *shell_command =
            "if  [ -n \"${DLB_ARGS##*--drom=1*}\" ]       || "
                "[ -n \"${DLB_ARGS##*--preinit-pid=*}\" ] || "
                "[ -n \"${DLB_ARGS##*--opt=foo*}\" ]      || "
                "[ ${OMP_NUM_THREADS:=0} != 1 ] ; "
            "then exit 1 ; fi";

        if (env) {
            execle("/bin/sh", "sh", "-c", shell_command, NULL, *env);
        } else {
            execl("/bin/sh", "sh", "-c", shell_command, NULL);
        }
        perror("exec failed");
        exit(EXIT_FAILURE);
    }
    // Wait for all child processes
    int wstatus;
    while(wait(&wstatus) > 0) {
        if (!WIFEXITED(wstatus))
            exit(EXIT_FAILURE);
        int rc = WEXITSTATUS(wstatus);
        if (rc != 0) {
            printf("Child return status: %d\n", rc);
            exit(EXIT_FAILURE);
        }
    }

    assert( DLB_DROM_Attach()                                   == DLB_SUCCESS );
    assert( DLB_DROM_PostFinalize(pid, 0)                       == DLB_SUCCESS );
    assert( DLB_DROM_Detach()                                   == DLB_SUCCESS );
}

int main(int argc, char **argv) {
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(0, &mask);
    setenv("OMP_NUM_THREADS", "4", 1);  // 4 should be reduced to 1 because of the mask
    setenv("DLB_ARGS", "--opt=foo", 1); // to ensure we don't overwrite existing opts

    /* Fork with CURRENT environment */
    test_fork(&mask, NULL);

    /* Fork with CUSTOM environment */
    char **new_environ = malloc(3*sizeof(char*)); // 2 variables + NULL
    // OMP_NUM_THREADS
    const char *var1 = "OMP_NUM_THREADS=4";
    const size_t var1len = strlen(var1) + 1;
    new_environ[0] = malloc(var1len);
    snprintf(new_environ[0], var1len, "%s", var1);
    // DLB_ARGS
    const char *var2 = "DLB_ARGS=--opt=foo";
    const size_t var2len = strlen(var2) + 1;
    new_environ[1] = malloc(var2len);
    snprintf(new_environ[1], var2len, "%s", var2);
    //
    new_environ[2] = NULL;
    test_fork(&mask, &new_environ);
    free(new_environ[0]);
    free(new_environ[1]);
    free(new_environ);

    return 0;
}
