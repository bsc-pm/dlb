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

/* DPD.h: Declaraciones */

#ifndef _DPD_H_
#define _DPD_H_

#define SIZEVENT 300
#define POL 1

/* Devuelve 0 si no es inicio de periodo y != (igual al valor de la muestra) si
es inicio de periodo */
/* muestra= valor de traceo ,en periode te devuelve la longitud del periodo */
int DPD(long muestra, int *periode);

/* Ventana maxima que mirara */
void DPDWindowSize(int max);

extern int max_period ;

#else
#endif









