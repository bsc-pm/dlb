#ifndef MYTIME_H
#define MYTIME_H

#include <time.h>

int diff_time(struct timespec init, struct timespec end, struct timespec* diff);
void add_time(struct timespec t1, struct timespec t2, struct timespec* sum);
void reset(struct timespec *t1);
double to_secs(struct timespec t1);

#endif // MYTIME_H

