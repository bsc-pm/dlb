/*************************************************************************************/
/*      Copyright 2012 Barcelona Supercomputing Center                               */
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

typedef struct{
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
} BalancePolicy;

extern int use_dpd;

void bind_master();

void Init(void);

void Finish(void);

void IntoCommunication(void);

void OutOfCommunication(void);

void IntoBlockingCall(int is_iter, int is_single);

void OutOfBlockingCall(int is_iter);

void updateresources( int max_resources );

void returnclaimed( void );

int tracing_ready();
#endif //DLB_KERNEL_H
