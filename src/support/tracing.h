/*********************************************************************************/
/*  Copyright 2015 Barcelona Supercomputing Center                               */
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

/********* EXTRAE EVENTS *************/
#define THREADS_USED_EVENT 800000
#define RUNTIME_EVENT      800020
#define EVENT_USER        0
#define EVENT_INIT        1
#define EVENT_INTO_MPI    2
#define EVENT_OUT_MPI     3
#define EVENT_UPDATE      4
#define EVENT_RETURN      5
#define EVENT_RELEASE_CPU 6
#define EVENT_CLAIM_CPUS  7
#define EVENT_RETURN_CPU  8
#define EVENT_LEND        9
#define EVENT_RETRIEVE    10
#define EVENT_RESET       11
#define EVENT_ACQUIRE_CPU 12

#define IDLE_CPUS_EVENT    800030
#define ITERATION_EVENT    800040
#define DLB_MODE_EVENT     800050
#define EVENT_ENABLED        1
#define EVENT_DISABLED       2
#define EVENT_SINGLE         3

/*************************************/

#ifdef INSTRUMENTATION_VERSION
void add_event( unsigned type, long long value );
void init_tracing( void );
#else
#define add_event(type, value)
#define init_tracing()
#endif


#ifdef INSTRUMENTATION_VERSION
#define DLB_INSTR(f) f
#else
#define DLB_INSTR(f)
#endif
