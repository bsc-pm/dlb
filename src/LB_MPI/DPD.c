/*********************************************************************************/
/*  Copyright 2009-2018 Barcelona Supercomputing Center                          */
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

/* FICHERO PARA DETECTAR LA PERIODICIDAD */

#ifdef MPI_LIB

#include "DPD.h"

#include <stdio.h>
#include <stdlib.h>

int inicializar_lista=0;
int max_period ;

typedef struct lista {
    long distance;
    int num_zeros;
    struct lista *next, *prev;
} LISTA;

LISTA *index_ptr[SIZEVENT];

typedef struct datasequence {
    long data;
    struct datasequence *next, *prev;
} DATASEQUENCE;

DATASEQUENCE *datasequence_ptr;


int DPD(long muestra, int *periode) {
    void build_lista(int);

    LISTA *work_ptr;
    DATASEQUENCE *data_ptr;



    int M,N;

    int count_max;
    int count_zeros;
    //int max_count;

    long addr;
    long zz;

    static int counter;

    int i,w;

    int limit, limit1;
    int flag;



    static int reset=1;
    static int period_old;
    static long addr_old;
    static long sample_counter;

    M=max_period;
    N=max_period;
    if (reset==1) {
        counter=1;
        reset=0;
        period_old=0;
        addr_old=0;
    } else {
        counter+=1;
    }
    if (inicializar_lista==0) {
        build_lista(SIZEVENT);
        inicializar_lista=1;
        max_period=SIZEVENT;
    }
    /* desplazar pointer de distancematrix y sequence */
    for (i=0; i<max_period; i++) {
        index_ptr[i]=index_ptr[i]->next;
    }
    datasequence_ptr=datasequence_ptr->next;
    if (reset==1) {
        counter=1;
        reset=0;
        period_old=0;
        addr_old=0;
    } else {
        counter+=1;
    }
    limit=counter-1;
    if (limit >= M-1) { limit=M-1; }
    datasequence_ptr->data=muestra;
    data_ptr=datasequence_ptr;
    if (counter>N-1) { limit1=N-1; }
    else { limit1=counter-1; }
    for (i=1; i<=limit1; i++) { /*i<=limit */
        zz=abs(datasequence_ptr->data - data_ptr->prev->data);
        data_ptr=data_ptr->prev;
        index_ptr[i]->distance=zz;
        index_ptr[i]->num_zeros=0;
    }
    addr=0;
    *periode=0;
    flag=0;
    w=0;
    while (flag==0  &&   w <1 ) {
        w++;
        count_max=0;
        count_zeros=0;
        //max_count=0;
        for (i=1; i<=limit; i++) {
            /* j=N; */
            count_zeros=0;
            if (index_ptr[i]->distance < 0.000000001) {
                work_ptr=index_ptr[i];
                count_zeros=work_ptr->prev->num_zeros+1;
                work_ptr->num_zeros=count_zeros;
                if (count_zeros>2*count_max  ) {
                    /* if (2*i > count_zeros) */
                    *periode=i;
                    flag=1;
                    count_max=count_zeros;
                }
            }
        }
    }
    work_ptr=index_ptr[*periode];
    addr=0;
    /*
    fprintf(stderr,"flag=%d wp=%d, p=%d \n",flag,work_ptr->num_zeros,*periode );
    */
    if (work_ptr->num_zeros < *periode) {
        flag=0;
        *periode =0;
    }
    if (flag==1) { /* ha detectado periodo */
        data_ptr=datasequence_ptr;
        for (i=1; i<*periode; i++) {
            data_ptr=data_ptr->prev;
        }
        addr=muestra;
        if (*periode!=period_old) {
            period_old=*periode;
            sample_counter=-1;
            addr_old=data_ptr->data;
            if (addr_old!=muestra) {
                addr=0;
            }
        }
        /* periode no ha cambiado */
        else if (addr_old!=muestra || sample_counter%period_old!=0) {
            addr=0;
        }
        /*
         fprintf(stderr,"p_o=%d, p=%d sc=%ld a0=%ld\n"
        ,period_old,*periode,sample_counter,addr_old);
        */
        if (*periode==period_old) { sample_counter+=1; }
        /* fprintf(stderr,"detecta per: i=%d periode=%d count=%d, addr=%ld\n",i, *periode,counter,addr); */
    }

    /* RESETAR: Ha cambiado el periodo */

    if (*periode==0)    {
        /* counter=1; */
        period_old=0;
        addr_old=0;
    }
    return(addr);
}


void build_lista(int sizelista) {
    LISTA *first[SIZEVENT], *successor[SIZEVENT];
    DATASEQUENCE *datafirst, *datasuccessor;

    int i,j;
    /* printf ("GENERARE Listas\n"); */
    /* LISTA PARA DISTANCEMATRIX */
    for (i=0; i<sizelista; i++) {
        first[i]=(LISTA *) malloc(sizeof(LISTA));
        first[i]->distance=10000000;
        first[i]->num_zeros=0;
    }

    for (i=0; i<sizelista; i++) {
        index_ptr[i]=first[i];
    }

    for (i=0; i<1; i++) {
        for (j=0; j<sizelista; j++) {
            successor[j]=(LISTA *) malloc(sizeof(LISTA));
            successor[j]->distance=1000000;
            successor[j]->num_zeros=0;
            index_ptr[j]->next=successor[j];
            index_ptr[j]->next->prev=index_ptr[j];
            index_ptr[j]=successor[j];
        }
    }
    for (i=0; i<sizelista; i++) {
        index_ptr[i]->next=first[i];
        first[i]->prev=index_ptr[i];
    }

    /* LISTA PARA SEQUENCE */

    datafirst=(DATASEQUENCE *) malloc(sizeof(DATASEQUENCE));
    datafirst->data=0;
    datasequence_ptr=datafirst;

    for (i=0; i<sizelista; i++) {
        datasuccessor=(DATASEQUENCE *) malloc(sizeof(DATASEQUENCE));
        datasuccessor->data=0;
        datasequence_ptr->next=datasuccessor;
        datasequence_ptr->next->prev=datasequence_ptr;
        datasequence_ptr=datasuccessor;
    }

    datasequence_ptr->next=datafirst;
    datafirst->prev=datasequence_ptr;

}


void DPDWindowSize(int max) {
    max_period = max;

}

#endif /* MPI_LIB */
