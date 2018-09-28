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
            __sync_synchronize();                       \
        }                                               \
        assert(expr);                                   \
    } while(0)

#endif /* ASSERT_LOOP_H */
