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

#include "support/debug.h"
#include "support/env.h"
#include "support/options.h"

#include <string.h>

int main( int argc, char **argv ) {

    /* verbose function may be invoked before DLB_Init */
    dlb_setenv("DLB_ARGS", "--verbose=drom", NULL, ENV_APPEND);
    verbose(VB_DROM, "This message should be visible");
    verbose(VB_TALP, "This one should not");

    options_t options;
    options_init(&options, "--verbose-format=node:thread");
    debug_init(&options);
    info("hello test %s", "!!");

    options_init(&options, "--verbose-format=:");
    debug_init(&options);
    info("hello test %s", "!!");

    options_init(&options, "--verbose-format=node:");
    debug_init(&options);
    info("hello test %s", "!!");

    print_backtrace();

    /*** printbuffer ***/
    print_buffer_t b;

    /* init */
    printbuffer_init(&b);
    assert( b.addr != NULL );
    assert( b.size > 0 );
    assert( b.len == 0 );
    assert( strcmp(b.addr, "") == 0 );
    printbuffer_destroy(&b);

    /* append */
    printbuffer_init(&b);
    printbuffer_append(&b, "hello");
    assert( strcmp(b.addr, "hello\n") == 0 );
    printbuffer_append(&b, "world");
    assert( strcmp(b.addr, "hello\nworld\n") == 0 );
    printbuffer_destroy(&b);

    /* append_no_newline */
    printbuffer_init(&b);
    printbuffer_append_no_newline(&b, "hello");
    assert( strcmp(b.addr, "hello") == 0 );
    printbuffer_append_no_newline(&b, "world");
    assert( strcmp(b.addr, "helloworld") == 0 );
    printbuffer_destroy(&b);

    /* growth */
    char large[5000];
    memset(large, 'A', sizeof(large) - 1);
    large[sizeof(large) - 1] = '\0';
    printbuffer_init(&b);
    printbuffer_append(&b, large);
    assert( strlen(b.addr) == strlen(large) + 1 ); // + '\n'
    assert( b.size >= strlen(b.addr) + 1 );
    printbuffer_destroy(&b);

    /* destroy */
    printbuffer_init(&b);
    printbuffer_append(&b, "test");
    printbuffer_destroy(&b);
    assert( b.addr == NULL );
    assert( b.size == 0 );
    assert( b.len == 0 );

    return 0;
}
