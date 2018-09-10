/*********************************************************************************/
/*  Copyright 2009-2018 Barcelona Supercomputing Center                          */
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

#ifndef ASSERT_LOOP_H
#define ASSERT_LOOP_H

#include <assert.h>

enum { ASSERT_LOOP_MAX_ITS = 100 };
enum { ASSERT_LOOP_USECS = 1000 };
#define assert_loop(expr)                               \
    do {                                                \
        int _ii;                                        \
        for(_ii=0; _ii<ASSERT_LOOP_MAX_ITS; ++_ii) {    \
            if (expr) break;                            \
            usleep(ASSERT_LOOP_USECS);                  \
        }                                               \
        assert(expr);                                   \
    } while(0);

#endif /* ASSERT_LOOP_H */
