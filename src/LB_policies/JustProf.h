#ifndef JUSTPROF_H
#define JUSTPROF_H

void JustProf_Init(int me, int num_procs, int node);

void JustProf_Finish(void);

void JustProf_IntoCommunication(void);

void JustProf_OutOfCommunication(void);

void JustProf_IntoBlockingCall(int is_iter);

void JustProf_OutOfBlockingCall(int is_iter);

void JustProf_UpdateResources();

#endif //JUSTPROF_H

