#include <comm.h>

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <sys/select.h>
#include <sys/time.h>
#include <assert.h>
#include <strings.h>
#include <unistd.h>


static struct sockaddr_in master; 
int *slaves;
fd_set fds;
int maxFd;
int dlb_port=60000;
int fd_local;

int num_slaves;
int me, node;

in_addr_t get_addr(char *name);
void LBrec(int fd,char *buff,int size);
int LBsend(int fd,char *buff,int size);

void LoadCommConfig(int num_procs, int meId, int nodeId){
#ifdef debugConfig
	fprintf(stderr,"%d:%d - Loading Common Configuration\n", node, me);
#endif	
	master.sin_family=PF_INET;
	master.sin_port=htons(dlb_port);
	master.sin_addr.s_addr=get_addr("localhost");
	bzero(&(master.sin_zero),8);

	num_slaves=num_procs;
	me = meId;
	node = nodeId;
}


void StartMasterComm(){
#ifdef debugConfig
	fprintf(stderr,"%d:%d - Loading Master Configuration\n", node, me);
#endif

	int rank=0;
	int fd_sock, fd_slave;
	int sin_size;
	int s=0;
	struct sockaddr_in slave;

	slaves=malloc(sizeof(int)*num_slaves);

	FD_ZERO(&fds);
	maxFd=0;

	while(rank<num_slaves){
		slaves[rank]=-1;
		rank++;
	}

	// Socket
	if ((fd_sock=socket(PF_INET,SOCK_STREAM,0))==-1){
		perror("CreateMasterSocket:: socket \n");
		exit(1);
	}
	// Bind
	if (bind(fd_sock,(struct sockaddr *)&master,sizeof(struct sockaddr))==-1){
		perror("CreateMasterSOcket:: Bind\n");
		exit(1);
	}
	//listen
	
	if (listen(fd_sock,num_slaves)==-1){
		perror("CreateMasterSocket:: listen\n");
		exit(1);
	}
	// Accept
	sin_size=sizeof(struct sockaddr);

	while(s<num_slaves){
		if ((fd_slave=accept(fd_sock,(struct sockaddr *)&slave,(socklen_t *)&sin_size))==-1){
			perror("CreateMasterSocket:: accept\n");
			exit(1);
		}else{ // Slave connected
			LBrec(fd_slave,(char*)&rank,sizeof(int));
#ifdef debugConfig
	fprintf(stderr,"%d:%d - Master: Slave[%d] connected to %d \n", node, me, rank, fd_slave);
#endif
			slaves[rank]=fd_slave;
			FD_SET(fd_slave, &fds);
			if (fd_slave>maxFd) maxFd=fd_slave;
		}
		s++;
	}
}

void StartSlaveComm(){
#ifdef debugConfig
	fprintf(stderr,"%d:%d - Loading Slave Configuration\n", node, me);
#endif
	int fdi, err;

	if ((fdi=socket(PF_INET,SOCK_STREAM,0))==-1){
		perror("CreateSlaveSocket:socket\n");
		exit(1);
	}

	do{
		 err=connect(fdi,(struct sockaddr *)&master,sizeof(struct sockaddr));
	}while ((err==-1) && (errno==ECONNREFUSED));


	if (err==-1){
		perror("CreateSlaveSocket:connect\n");
		exit(1);
	}
	
	if (LBsend(fdi,(char *)&me,sizeof(int))<0){
		fprintf(stderr,"CreateSlaveSocket: LBSend Error at %d\n", me);
	}
#ifdef debugConfig
	fprintf(stderr,"%d:%d - Slave: Sending %d to Master\n", node, me, me);
#endif

	fd_local=fdi;
}

in_addr_t get_addr(char *name) 
{
	struct hostent *h;
	if ((h=gethostbyname(name))==NULL){
		perror("get_addr: error getting address\n");
		exit(1);
	}
	return *((in_addr_t *)h->h_addr);
}

void GetFromMaster(char *info,int size)
{
#ifdef debugComm
	fprintf(stderr,"%d:%d - Slave[%d] Receiving from Master at %d\n", node, me, me, fd_local);
#endif	
	
	while (fd_local==0);
	LBrec(fd_local,info,size);
}

void SendToMaster(char *info,int size)
{
#ifdef debugComm
	fprintf(stderr,"%d:%d - Slave[%d] Sending to Master at %d\n", node, me, me, fd_local);
#endif	

	while (fd_local==0);
	if (LBsend(fd_local,info,size)<0){
		fprintf(stderr,"%d:%d - SendToMaster: LBSend Error at %d\n", node, me, fd_local);
	}
}

void GetFromSlave(int rank,char *info,int size)
{
#ifdef debugComm
	fprintf(stderr,"%d:%d - Master Receiving from Slave[%d] at %d\n", node, me, rank, slaves[rank]);
#endif	
	LBrec(slaves[rank],info,size);
}

void SendToSlave(int rank,char *info,int size)
{
#ifdef debugComm
	fprintf(stderr,"%d:%d - Master Sending to Slave[%d] at %d\n", node, me, rank, slaves[rank]);
#endif	
	if (LBsend(slaves[rank],info,size)<0){
		fprintf(stderr,"%d:%d - SendToSlave: LBSend Error at %d\n", node, me, slaves[rank]);
	}
}

void FillFdSet(){
	FD_ZERO(&fds);

        int i;
	for(i=0; i<num_slaves ; i++){
		FD_SET(slaves[i], &fds);
	}
}

int GetFromAnySlave(char *info,int size)
{
#ifdef debugComm
	fprintf(stderr,"%d:%d - Master Receiving from any Slave\n", node, me);
#endif
	int num_fds, i;
	if ((num_fds = select(maxFd+1, &fds, NULL, NULL, NULL))<0){
		perror("select()");
	}

	i=0;
	while(!FD_ISSET(slaves[i], &fds)){
		i++;
	}
	LBrec(slaves[i],info,size);	
	FillFdSet();
	return i;
}

int pendingSlaves(){
	int num_fds;
	struct timeval timeout;
	timeout.tv_sec=0;
	timeout.tv_usec=0;

	if ((num_fds = select(maxFd+1, &fds, NULL, NULL, &timeout))<0){
		perror("select()");
	}

	FillFdSet();
	return num_fds;
}
///////////////////////////////
//Basic operations
///////////////////////////////

void LBrec(int fd,char *buff,int size)
{
	int total;
	int pendings=size;
	do{
		while (((total=read(fd,buff,size))==-1) && (errno==EINTR));
		pendings-=total;
		buff+=total;
	}while (pendings);
}


int LBsend(int fd,char *buff,int size)
{
	int total;
	int pendings=size;

	do{
		while (((total=write(fd,buff,size))==-1) && (errno==EINTR));
		if (total == -1){
			perror("LBsend");
			return -1;
		}
		pendings-=total;
		buff+=total;
	}while (pendings);
	return 0;
}
