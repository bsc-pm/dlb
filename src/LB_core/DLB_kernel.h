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

#ifndef DLB_KERNEL_H
#define DLB_KERNEL_H

#include <stdbool.h>

typedef struct {
    void (*init) (void);
    void (*finish) (void);
    void (*initIteration) (void);
    void (*finishIteration) (void);
    void (*intoCommunication) (void);
    void (*outOfCommunication) (void);
    void (*intoBlockingCall) (int is_iter, int blocking_mode);
    void (*outOfBlockingCall) (int is_iter);
    void (*updateresources) ( int max_resources );
    void (*returnclaimed) (void);
    int (*releasecpu) (int cpu);
    int (*returnclaimedcpu) (int cpu);
    void (*claimcpus) (int cpus);
    void (*acquirecpu) (int cpu);
    int (*checkCpuAvailability) (int cpu);
    void (*resetDLB) (void);
    void (*disableDLB)(void);
    void (*enableDLB) (void);
    void (*single) (void);
    void (*parallel) (void);
} BalancePolicy;

extern int use_dpd;

void set_dlb_enabled(bool enabled);

void load_modules(void);

int Initialize(void);

void Finish(int id);

void Terminate(void);

void Update(void);

void IntoCommunication(void);

void OutOfCommunication(void);

void IntoBlockingCall(int is_iter, int is_single);

void OutOfBlockingCall(int is_iter);

void updateresources( int max_resources );

void returnclaimed( void );

int releasecpu( int cpu );

int returnclaimedcpu( int cpu );

void claimcpus( int cpus );

int checkCpuAvailability ( int cpu );

int tracing_ready();

void resetDLB ();

int is_auto( void );

void acquirecpu (int cpu);

void singlemode(void);

void parallelmode(void);

void printShmem(void);

#endif //DLB_KERNEL_H
