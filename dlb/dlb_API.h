#ifndef DLB_API_H
#define DLB_API_H

typedef struct{
	void (*init) (int me, int num_procs, int node);
	void (*finish) (void);
	void (*initIteration) (void);
	void (*finishIteration) (double cpuSecs, double MPISecs);
	void (*intoCommunication) (void);
	void (*outOfCommunication) (void);
	void (*intoBlockingCall) (double cpuSecs, double MPISecs);
	void (*outOfBlockingCall) (void);
	void (*updateresources) (void);
} BalancePolicy;

void Init(int me, int num_procs, int node);

void Finish(void);

void InitIteration(void);

void FinishIteration(void);

void IntoCommunication(void);

void OutOfCommunication(void);

void IntoBlockingCall(void);

void OutOfBlockingCall(void);

void dummyFunc(void);
void dummyInit(int me, int num_procs, int node);
void dummyFinishIter(double a, double b);
void dummyIntoBlockingCall(double a, double b);

void updateresources();
void UpdateResources();

#endif //DLB_API_H