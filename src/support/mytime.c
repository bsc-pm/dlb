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

#include <time.h>

int diff_time( struct timespec init, struct timespec end, struct timespec* diff )
{
   if ( init.tv_sec > end.tv_sec ) {
      return -1;
   } else {
      if ( ( init.tv_sec == end.tv_sec ) && ( init.tv_nsec > end.tv_nsec ) ) {
         return -1;
      } else {
         if ( init.tv_nsec > end.tv_sec ) {
            diff->tv_sec = end.tv_sec - ( init.tv_sec + 1 );
            diff->tv_nsec = ( end.tv_nsec + 1e9 ) - init.tv_nsec;
         } else {
            diff->tv_sec = end.tv_sec - init.tv_sec;
            diff->tv_nsec = end.tv_nsec - init.tv_nsec;
         }
      }
   }

   return 0;
}

void add_time( struct timespec t1, struct timespec t2, struct timespec* sum )
{
   sum->tv_nsec = t1.tv_nsec + t2.tv_nsec;

   if ( sum->tv_nsec >= 1e9 ) {
      sum->tv_sec = t1.tv_sec + t2.tv_sec + ( sum->tv_nsec/1e9 );
      sum->tv_nsec = ( sum->tv_nsec % ( long )1e9 );
   } else {
      sum->tv_sec = t1.tv_sec + t2.tv_sec;
   }
}

void mult_time( struct timespec t1, int factor, struct timespec* prod )
{
   prod->tv_nsec = t1.tv_nsec * factor;

   if ( prod->tv_nsec >= 1e9 ) {
      prod->tv_sec = (t1.tv_sec * factor) + ( prod->tv_nsec/1e9 );
      prod->tv_nsec = ( prod->tv_nsec % ( long )1e9 );
   } else {
      prod->tv_sec = t1.tv_sec * factor;
   }
}

void reset( struct timespec *t1 )
{
   t1->tv_nsec = 0;
   t1->tv_sec = 0;
}

double to_secs( struct timespec t1 )
{
   double secs;

   secs = t1.tv_sec;
   secs += ( t1.tv_nsec ) / 1e9;

   return secs;
}
