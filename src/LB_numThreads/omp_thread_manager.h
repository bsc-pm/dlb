/*********************************************************************************/
/*  Copyright 2009-2019 Barcelona Supercomputing Center                          */
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
/*  along with DLB.  If not, see <http://www.gnu.org/licenses/>.                 */
/*********************************************************************************/

#ifndef OMP_THREAD_MANAGER_H
#define OMP_THREAD_MANAGER_H

#include "support/options.h"

void omp_thread_manager__init(const options_t *options);
void omp_thread_manager__finalize(void);
void omp_thread_manager__borrow(void);
void omp_thread_manager__lend(void);
void omp_thread_manager__IntoBlockingCall(void);
void omp_thread_manager__OutOfBlockingCall(void);

#endif /* OMP_THREAD_MANAGER_H */
