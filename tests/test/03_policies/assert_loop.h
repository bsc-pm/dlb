#ifndef ASSERT_LOOP_H
#define ASSERT_LOOP_H

#include <assert.h>

#define MAX_ITS 100
#define USECS 1000
#define assert_loop(expr)                   \
    do {                                    \
        int _ii;                            \
        for(_ii=0; _ii<MAX_ITS; ++_ii) {    \
            if (expr) break;                \
            usleep(USECS);                  \
        }                                   \
        assert(expr);                       \
    } while(0);

#endif /* ASSERT_LOOP_H */
