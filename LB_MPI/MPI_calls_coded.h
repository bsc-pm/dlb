#ifndef MPI_CALLS_CODED_H
#define MPI_CALLS_CODED_H

#define is_blocking(x) (x) & 0x10

enum type_mpi_call{
	_Unknown=0,
	_Send=1,
	_Receive=2,
	_Barrier=3,
	_Wait=4,
	_Bcast=5,
	_All2All=6,
	_Gather=7,
	_Scatter=8,
	_Scan=9,
	_Reduce=10,
	_SendRecv=11,
//Last 16 bits: 0000000x yyyyyyyy
// x : Blocking - No blocking
// y : type mpi call [0-15]
	_Blocking=0x10
};

typedef enum{
	Unknown=0,
	Bsend=_Send|_Blocking,
	Ssend=_Send|_Blocking,
	Rsend=_Send|_Blocking,
	Send=_Send|_Blocking,
	Ibsend=_Send,
	Isend=_Send,
	Issend=_Send,
	Irsend=_Send,
	Recv=_Receive|_Blocking,
	Irecv=_Receive,
	Reduce=_Reduce|_Blocking,
	Reduce_scatter=_Reduce|_Blocking,
	Allreduce=_Reduce|_Blocking,
	Barrier=_Barrier|_Blocking,
	Wait=_Wait|_Blocking,
	Waitall=_Wait|_Blocking,
	Waitany=_Wait|_Blocking,
	Waitsome=_Wait|_Blocking,
	Bcast=_Bcast|_Blocking,
	Alltoall=_All2All|_Blocking,
	Alltoallv=_All2All|_Blocking,
	Allgather=_Gather|_Blocking,
	Allgatherv=_Gather|_Blocking,
	Gather=_Gather|_Blocking,
	Gatherv=_Gather|_Blocking,
	Scatter=_Scatter|_Blocking,
	Scatterv=_Scatter|_Blocking,
	Scan=_Scan|_Blocking,
	SendRecv=_SendRecv|_Blocking,
} mpi_call;

#endif //MPI_CALLS_CODED_H

