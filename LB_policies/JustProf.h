#ifndef JUSTPROF_H
#define JUSTPROF_H

void JustProf_Init(void);

void JustProf_Finish(void);

void JustProf_InitIteration(void);

void JustProf_FinishIteration(void);

void JustProf_IntoCommunication(void);

void JustProf_OutOfCommunication(void);

void JustProf_IntoBlockingCall(void);

void JustProf_OutOfBlockingCall(void);

#endif //JUSTPROF_H