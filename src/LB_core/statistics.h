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

#ifndef STATISTICS_H
#define STATISTICS_H

void stats_init(void);
void stats_finalize(void);
void stats_update(void);

void stats_ext_init(void);
void stats_ext_finalize(void);
int stats_ext_getnumcpus(void);
void stats_ext_getpidlist(int *pidlist,int *nelems,int max_len);
double stats_ext_getcpuusage(int pid);
void stats_ext_getcpuusage_list(double *usagelist,int *nelems,int max_len);
int stats_ext_getactivecpus(int pid);
void stats_ext_getactivecpus_list(int *cpuslist,int *nelems,int max_len);
int stats_ext_getloadavg(int pid,double *load);

#endif /* STATISTICS_H */
