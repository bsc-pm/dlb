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
    test_generator_ENV=( "LB_TEST_MODE=single" )
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

void __gcov_flush() __attribute__((weak));

void test_fork(const cpu_set_t *mask, char ***environ) {
    pid_t pid = fork();
    assert( pid >= 0 );
    if (pid == 0) {
        pid = getpid();
        assert( DLB_DROM_Init()                         == DLB_SUCCESS );
        assert( DLB_DROM_PreInit(pid, mask, 1, environ) == DLB_SUCCESS );
        assert( DLB_DROM_Finalize()                     == DLB_SUCCESS );

        if (__gcov_flush) __gcov_flush();

        // Exec into a shell process to check if the environment is correctly modified
        const char *shell_command =
            "if  [ -z ${LB_PREINIT_PID+x} ]     || "
                "[ ${LB_DROM:=0} != 1 ]         || "
                "[ ${OMP_NUM_THREADS:=0} != 1 ] ; "
            "then exit 1 ; fi";

        if (environ) {
            execle("/bin/sh", "sh", "-c", shell_command, NULL, *environ);
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

    assert( DLB_DROM_Init()                 == DLB_SUCCESS );
    assert( DLB_DROM_PostFinalize(pid, 0)   == DLB_SUCCESS );
    assert( DLB_DROM_Finalize()             == DLB_SUCCESS );
}

int main(int argc, char **argv) {
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(0, &mask);
    setenv("OMP_NUM_THREADS", "4", 1); // 4 should be reduced to 1 because of the mask

    // Fork with CURRENT environment
    test_fork(&mask, NULL);

    // Fork with CUSTOM environment
    const char *var = "OMP_NUM_THREADS=4";
    const size_t varlen = strlen(var) + 1;
    char *np = malloc(varlen);
    snprintf(np, varlen, "%s", var);
    char **new_environ = malloc(2*sizeof(char*)); // 1 variable + NULL
    new_environ[0] = np;
    new_environ[1] = NULL;
    test_fork(&mask, &new_environ);

    return 0;
}
