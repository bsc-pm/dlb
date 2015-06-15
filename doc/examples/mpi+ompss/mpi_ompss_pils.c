/**************************************************************************/
/*      Copyright 2014 Barcelona Supercomputing Center                    */
/*                                                                        */
/*      This file is part of the DLB library.                             */
/*                                                                        */
/*      DLB is free software: you can redistribute it and/or modify       */
/*      it under the terms of the GNU Lesser General Public License as published by  */
/*      the Free Software Foundation, either version 3 of the License, or  */
/*      (at your option) any later version.                               */
/*                                                                        */
/*      DLB is distributed in the hope that it will be useful,            */
/*      but WITHOUT ANY WARRANTY; without even the implied warranty of    */
/*      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the     */
/*      GNU Lesser General Public License for more details.               */
/*                                                                        */
/*      You should have received a copy of the GNU Lesser General Public License  */
/*      along with DLB.  If not, see <http://www.gnu.org/licenses/>.      */
/**************************************************************************/

#include <sys/time.h>
#include <stdlib.h>
#include <stdio.h>
#include <mpi.h>

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

#pragma GCC diagnostic ignored "-Wstrict-aliasing"

const int DEF_LOADS_SIZE = 2;
const int default_loads[] = {100, 250};

int computation (int num, int usec) {
    float a=0.99999f;
    float p=num;
    int i, x;
    x=145*usec;

    for(i=0; i<x; i++){
        p+=a*i;
        p=p/i;
    }

    return (int)p;
}

long usecs (void) {
    struct timeval t;
    gettimeofday(&t,NULL);
    return t.tv_sec*1000000+t.tv_usec;
}

double LoadUnbalance(int loads[], int how_many) {
    int sum=0;
    int max=0;
    int i;
    for (i=0; i<how_many; i++){
        sum+=loads[i];
        max=MAX(max, loads[i]);
    }
    return sum / (how_many*max);
}

#pragma omp task
void iter(double * app_time, int *fib, int usec) {
    long t_start,t_end;

    t_start=usecs();
    *fib=computation(35, usec);
    t_end=usecs();

    double iter_time=(t_end-t_start)/1000000.0;
    #pragma omp atomic
    *app_time += iter_time;
}

int main(int argc, char* argv[]) {
    if (argc!=5) {
        fprintf(stderr, "Error: Wrong number of parameters. pils <loads file> <parallelism grain [0..1] <loops> <task duration>\n");
        fprintf(stderr, "%d \n", argc);
        exit(EXIT_FAILURE);
    }

    long t_start,t_end;
    t_start=usecs();

    MPI_Init(&argc,&argv);

    int me, how_many;
    MPI_Comm_rank(MPI_COMM_WORLD, &me);
    MPI_Comm_size(MPI_COMM_WORLD, &how_many);

    int loads[how_many];
    int steps;
    int BS;
    int tope, fib;
    double iter_time=0;
    double final_time, app_time;
    double parall_grain = atof(argv[2]);
    int loops=atoi(argv[3]);
    int usec=atoi(argv[4]);

    if (parall_grain>1 || parall_grain<=0) {
        fprintf(stderr, "Error: Parallelism grain parameter must be a value between 1 and 0 (but not 0). %f\n", parall_grain);
        exit(EXIT_FAILURE);
    }

    if (me==0) {
        FILE* fileLoads = fopen(argv[1],"r");

        if(fileLoads==NULL) {
            perror("Fopen file of loads");
            fprintf(stderr,"(%s)\n",argv[1]);
            exit(EXIT_FAILURE);
        }

        int i;
        for (i=0; i<how_many; i++) {
            if (fscanf(fileLoads, "%d ", &loads[i]) == EOF ) break;
        }

        for (;i<how_many; i++) {
            loads[i] = default_loads[i%DEF_LOADS_SIZE];
        }

        if(i<how_many) fprintf(stderr, "ERROR: Bad format in unbalance file\n");

        double LB = LoadUnbalance(loads, how_many);
        fprintf(stdout,"Unbalance: %.2f \n", LB);
    }

    MPI_Scatter(&loads, 1, MPI_INT, &steps, 1, MPI_INT, 0, MPI_COMM_WORLD);
    fprintf(stdout,"%d: Load = %d\n", me, steps);

    BS = steps * parall_grain;
    if (BS<=0) BS=1;

    fprintf(stdout,"%d: BS = %d\n", me, BS);

    /***********************************************************************/
    /************** MAIN LOOP **********************************************/
    int k;
    for(k=0; k<loops; k++) {

        int i;
        for (i=0; i<steps; i+=BS) {
            tope=MIN(steps, i+BS);

            int j;
            for(j=i; j<tope; j++){
                iter(&iter_time, &fib, usec);
            }
        }
        #pragma omp taskwait

        // Total computation time for each process
        printf("%d: Total time: %6.3f\n",me, iter_time);

        MPI_Barrier(MPI_COMM_WORLD);

        // Add computation time of whole application
        MPI_Allreduce( &iter_time, &final_time, 1, MPI_DOUBLE , MPI_SUM, MPI_COMM_WORLD );
    }

    if(me==0) printf("Final time: %6.3f\n", final_time);

    // Application elapsed time
    t_end=usecs();
    app_time = ((double)(t_end-t_start))/1000000;
    if (me ==0) printf("\nApplication time = %f \n", app_time);

    MPI_Finalize();

    return EXIT_SUCCESS;
}
