/*********************************************************************************/
/*  Copyright 2015 Barcelona Supercomputing Center                               */
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

/* from man 7 signal:
 *
 *  POSIX.1-1990 signals
 *
 *     Signal     Value     Action   Comment
 *     ----------------------------------------------------------------------
 *     SIGHUP        1       Term    Hangup detected on controlling terminal
 *                                   or death of controlling process
 *     SIGINT        2       Term    Interrupt from keyboard
 *     SIGQUIT       3       Core    Quit from keyboard
 *     SIGILL        4       Core    Illegal Instruction
 *     SIGABRT       6       Core    Abort signal from abort(3)
 *     SIGFPE        8       Core    Floating point exception
 *     SIGKILL       9       Term    Kill signal
 *     SIGSEGV      11       Core    Invalid memory reference
 *     SIGPIPE      13       Term    Broken pipe: write to pipe with no readers
 *     SIGALRM      14       Term    Timer signal from alarm(2)
 *     SIGTERM      15       Term    Termination signal
 *     SIGUSR1      10       Term    User-defined signal 1
 *     SIGUSR2      12       Term    User-defined signal 2
 *     SIGCHLD      17       Ign     Child stopped or terminated
 *     SIGCONT      18       Cont    Continue if stopped
 *     SIGSTOP      19       Stop    Stop process
 *     SIGTSTP      20       Stop    Stop typed at terminal
 *     SIGTTIN      21       Stop    Terminal input for background process
 *     SIGTTOU      22       Stop    Terminal output for background process
 *
 *  POSIX.1-2001 signals
 *
 *     Signal       Value     Action   Comment
 *     --------------------------------------------------------------------
 *     SIGBUS         7        Core    Bus error (bad memory access)
 *     SIGPOLL                 Term    Pollable event (Sys V). Synonym for SIGIO
 *     SIGPROF        27       Term    Profiling timer expired
 *     SIGSYS         31       Core    Bad argument to routine (SVr4)
 *     SIGTRAP        5        Core    Trace/breakpoint trap
 *     SIGURG         23       Ign     Urgent condition on socket (4.2BSD)
 *     SIGVTALRM      26       Term    Virtual alarm clock (4.2BSD)
 *     SIGXCPU        24       Core    CPU time limit exceeded (4.2BSD)
 *     SIGXFSZ        25       Core    File size limit exceeded (4.2BSD)
 *
 *  The signals SIGKILL and SIGSTOP cannot be caught
 */


#include <stddef.h>
#include <signal.h>
#include "support/debug.h"
#include "support/types.h"
#include "support/options.h"

#define MAX_SIGNUM 32

static struct sigaction new_action;
static struct sigaction old_actions[MAX_SIGNUM];
static bool registed_signals[MAX_SIGNUM] = {false};
static void (*terminate)(void) = NULL;

static void termination_handler( int signum, siginfo_t *si, void *context ) {
    info( "Signal caught %d. Cleaning up DLB", signum );
    terminate();

    /* Restore original action, then raise it */
    sigaction( signum, &old_actions[signum], NULL );
    raise( signum );
}

static void register_signal( int signum ) {
    fatal_cond0( !(signum >= 0 && signum < MAX_SIGNUM), "Registering unknown signal" );

    sigaction( signum, NULL, &old_actions[signum] );
    if ( old_actions[signum].sa_handler != SIG_IGN ) {
        sigaction( signum, &new_action, NULL );
    }

    registed_signals[signum] = true;
}

static void unregister_signal( int signum ) {
    fatal_cond0( !(signum >= 0 && signum < MAX_SIGNUM), "Unregistering unknown signal" );
    ensure( registed_signals[signum], "Unregistering a not registered signal" );

    sigaction( signum, &old_actions[signum], NULL );

    registed_signals[signum] = false;
}

void register_signals(const options_t *options, void(*terminate_fct)(void)) {
    if (options->debug_opts & DBG_REGSIGNALS) {
        warning("Debug option: register-signals.");

        /* Set terminate function */
        terminate = terminate_fct;

        /* Set up new handler */
        new_action.sa_sigaction = termination_handler;
        new_action.sa_flags = SA_SIGINFO;
        sigemptyset( &new_action.sa_mask );

        register_signal( SIGHUP );
        register_signal( SIGINT );
        register_signal( SIGQUIT );
        register_signal( SIGILL );
        register_signal( SIGABRT );
        register_signal( SIGFPE );
        register_signal( SIGSEGV );
        register_signal( SIGPIPE );
        register_signal( SIGALRM );
        register_signal( SIGTERM );
        register_signal( SIGUSR1 );
        //register_signal( SIGUSR2 );
        //register_signal( SIGCHLD );
        register_signal( SIGBUS );
        register_signal( SIGURG );
        register_signal( SIGVTALRM );
        register_signal( SIGXCPU );
        register_signal( SIGXFSZ );
    }
}

void unregister_signals(const options_t *options) {
    if (options->debug_opts & DBG_REGSIGNALS) {
        int i;
        for ( i=0; i<MAX_SIGNUM; i++ ) {
            if ( registed_signals[i] ) {
                unregister_signal(i);
            }
        }
    }
}
