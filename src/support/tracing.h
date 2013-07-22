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

/********* EXTRAE EVENTS *************/
#define THREADS_USED_EVENT 800000
#define RUNTIME_EVENT      800020
   #define EVENT_INIT        1
   #define EVENT_INTO_MPI    2
   #define EVENT_OUT_MPI     3
   #define EVENT_UPDATE      4
   #define EVENT_RETURN      5
#define ITERATION_EVENT    800021
#define IDLE_CPUS_EVENT    800030
/*************************************/

#ifdef INSTRUMENTATION_VERSION
void add_event( int type, int value );
#else
#define add_event(type, value)
#endif


#ifdef INSTRUMENTATION_VERSION
#define DLB_INSTR(f) f
#else
#define DLB_INSTR(f)
#endif
