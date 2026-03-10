/*********************************************************************************/
/*  Copyright 2009-2026 Barcelona Supercomputing Center                          */
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
/*  along with DLB.  If not, see <https://www.gnu.org/licenses/>.                */
/*********************************************************************************/

#ifndef PAPI_H
#define PAPI_H

#define PAPI_OK            0
#define PAPI_NULL         -1
#define PAPI_EINVAL       -1
#define PAPI_ENOMEM       -2
#define PAPI_ESYS         -3
#define PAPI_ECMP         -4
#define PAPI_ENOTRUN      -9

#define PAPI_NOT_INITED       0
#define PAPI_LOW_LEVEL_INITED 1

#define PAPI_VERSION_NUMBER(maj,min,rev,inc) (((maj)<<24) | ((min)<<16) | ((rev)<<8) | (inc))
#define PAPI_VERSION_MAJOR(x)     (((x)>>24)    & 0xff)
#define PAPI_VERSION_MINOR(x)     (((x)>>16)    & 0xff)
#define PAPI_VERSION_REVISION(x)  (((x)>>8)     & 0xff)
#define PAPI_VERSION_INCREMENT(x) ((x)          & 0xff)
#define PAPI_VERSION              PAPI_VERSION_NUMBER(7,2,0,0)
#define PAPI_VER_CURRENT (PAPI_VERSION & 0xffff0000)
#define PAPI_LIB_VERSION  21

#define PAPI_TOT_CYC       -2147483589
#define PAPI_TOT_INS       -2147483598

int  PAPI_library_init(int version);
int  PAPI_thread_init(unsigned long (*id_fn) (void));
int  PAPI_register_thread(void);
int  PAPI_is_initialized(void);
int  PAPI_unregister_thread(void);
void PAPI_shutdown(void);
int  PAPI_create_eventset(int *EventSet);
int  PAPI_add_events(int EventSet, int *Events, int number);
int  PAPI_cleanup_eventset(int EventSet);
int  PAPI_destroy_eventset(int *EventSet);
int  PAPI_start(int EventSet);
int  PAPI_stop(int EventSet, long long * values);
int  PAPI_read(int EventSet, long long * values);
int  PAPI_get_opt(int option, void * ptr);
char *PAPI_strerror(int);

#endif /* CUDA_RUNTIME_H */
