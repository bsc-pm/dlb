/*************************************************************************************/
/*      Copyright 2014 Barcelona Supercomputing Center                               */
/*                                                                                   */
/*      This file is part of the DLB library.                                        */
/*                                                                                   */
/*      DLB is free software: you can redistribute it and/or modify                  */
/*      it under the terms of the GNU Lesser General Public License as published by  */
/*      the Free Software Foundation, either version 3 of the License, or            */
/*      (at your option) any later version.                                          */
/*                                                                                   */
/*      DLB is distributed in the hope that it will be useful,                       */
/*      but WITHOUT ANY WARRANTY; without even the implied warranty of               */
/*      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                */
/*      GNU Lesser General Public License for more details.                          */
/*                                                                                   */
/*      You should have received a copy of the GNU Lesser General Public License     */
/*      along with DLB.  If not, see <http://www.gnu.org/licenses/>.                 */
/*************************************************************************************/

#include "support/mytime.h"
#include "support/debug.h"
#include "support/globals.h"
#include "support/tracing.h"

#include <stdio.h>


int iterNum;
struct timespec initAppl;
struct timespec initComp;
//struct timespec initCpu;
struct timespec initIter;

struct timespec cpuTime;
struct timespec compTime;

struct timespec iter_cpuTime;
struct timespec iter_compTime;

int myCPUS;
void JustProf_Init() {
    //Read Environment vars

    myCPUS = _default_nthreads;

    if (clock_gettime(CLOCK_REALTIME, &initAppl)<0) {
        fprintf(stderr, "DLB ERROR: clock_gettime failed\n");
    }

    if (clock_gettime(CLOCK_REALTIME, &initComp)<0) {
        fprintf(stderr, "DLB ERROR: clock_gettime failed\n");
    }
    reset(&cpuTime);
    reset(&compTime);
    reset(&iter_cpuTime);
    reset(&iter_compTime);
    iterNum=0;
}

void JustProf_Finish(void) {
    add_event(ITERATION_EVENT, 0);
    struct timespec fin;
    struct timespec aux;
    double totalTime;

    if (clock_gettime(CLOCK_REALTIME, &fin)<0) {
        fprintf(stderr, "DLB ERROR: clock_gettime failed\n");
    }

    diff_time(initComp, fin, &aux);


    add_time(compTime, aux, &compTime);

    mult_time(aux, myCPUS, &aux);

    add_time(cpuTime, aux, &cpuTime);

    diff_time(initAppl, fin, &aux);

    totalTime=to_secs(aux) * myCPUS;

    info("Application -> %.4f secs", to_secs(aux));
    info("Computation time: %.4f secs", to_secs(compTime));
    info("CPU time: %.4f secs", to_secs(cpuTime));

    info("Usage:  %.2f", (to_secs(cpuTime)*100)/totalTime);
}


void JustProf_IntoCommunication(void) {}

void JustProf_OutOfCommunication(void) {}

void JustProf_IntoBlockingCall(int is_iter, int blocking_mode) {
    struct timespec initMPI;
    struct timespec diff;

    if (clock_gettime(CLOCK_REALTIME, &initMPI)<0) {
        fprintf(stderr, "DLB ERROR: clock_gettime failed\n");
    }

    diff_time(initComp, initMPI, &diff);

    add_time(compTime, diff, &compTime);
    add_time(iter_compTime, diff, &iter_compTime);

    mult_time(diff, myCPUS, &diff);

    add_time(cpuTime, diff, &cpuTime);
    add_time(iter_cpuTime, diff, &iter_cpuTime);

}

void JustProf_OutOfBlockingCall(int is_iter) {
    if (clock_gettime(CLOCK_REALTIME, &initComp)<0) {
        fprintf(stderr, "DLB ERROR: clock_gettime failed\n");
    }
    if (is_iter!=0) {
        if(iterNum!=0) {
            //Finishing iteration
            struct timespec aux;
            double totalTime;

            diff_time(initIter, initComp, &aux);

            totalTime=to_secs(aux) * myCPUS;

            info("Iteration %d -> %.4f secs Usage: %.2f (%.4f * 100 / %.4f )", iterNum, to_secs(aux), (to_secs(iter_cpuTime)*100)/totalTime, to_secs(iter_cpuTime), totalTime );
        }
        //Starting iteration
        iterNum++;
        add_event(ITERATION_EVENT, iterNum);

        initIter=initComp;

        reset(&iter_cpuTime);
        reset(&iter_compTime);

        info("Iteration %d detected", iterNum);
    }
}

void JustProf_UpdateResources() {}
