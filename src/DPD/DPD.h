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
 

 






