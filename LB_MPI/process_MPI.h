#ifndef PROCESS_MPI_H
#define PROCESS_MPI_H

void before_init();
void after_init();
void before_mpi(int call, int buf, int dest);
void after_mpi();
void before_finalize();
void after_finalize();

#endif //PROCESS_MPI_H
