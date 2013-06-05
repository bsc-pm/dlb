#ifndef DLB_H
#define DLB_H

typedef struct{
	void (*init) (int me, int num_procs, int node);
	void (*finish) (void);
	void (*initIteration) (void);
	void (*finishIteration) (void);
	void (*intoCommunication) (void);
	void (*outOfCommunication) (void);
	void (*intoBlockingCall) (void);
	void (*outOfBlockingCall) (void);
	void (*updateresources) ( int max_resources );
} BalancePolicy;

extern int use_dpd;

void bind_master();

void Init(int me, int num_procs, int node);

void Finish(void);

void InitIteration(void);

void FinishIteration(void);

void IntoCommunication(void);

void OutOfCommunication(void);

void IntoBlockingCall(void);

void OutOfBlockingCall(void);

void updateresources( int max_resources );

int tracing_ready();
#endif //DLB_H
