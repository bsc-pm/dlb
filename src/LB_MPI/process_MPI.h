#ifndef PROCESS_MPI_H
#define PROCESS_MPI_H

#include <MPI_calls_coded.h>

void before_init(void);
void after_init(void);
void before_mpi(int call, int buf, int dest);
void after_mpi(mpi_call call_type);
void before_finalize(void);
void after_finalize(void);
void enable_mpi(void);
void disable_mpi(void);
void node_barrier(void);

#endif //PROCESS_MPI_H
