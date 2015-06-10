/*************************************************************************************/
/*      Copyright 2014 Barcelona Supercomputing Center                               */
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

#define _GNU_SOURCE        /* or _BSD_SOURCE or _SVID_SOURCE */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include "DLB_kernel.h"

#include <LB_policies/Lend_light.h>
#include <LB_policies/Weight.h>
#include <LB_policies/JustProf.h>
#include <LB_policies/Lewi_map.h>
#include <LB_policies/lewi_mask.h>
#include <LB_policies/autonomous_lewi_mask.h>
#include <LB_policies/RaL.h>
#include <LB_policies/PERaL.h>

#include <LB_numThreads/numThreads.h>
#include "LB_core/statistics.h"
#include "LB_core/drom.h"
#include "support/debug.h"
#include "support/globals.h"
#include "support/tracing.h"
#include "support/utils.h"
#include "support/mytime.h"
#include "support/mask_utils.h"
#include "support/sighandler.h"

//int iterNum = 0;
//struct timespec initAppl;
//struct timespec initComp;
//struct timespec initMPI;

//struct timespec iterCpuTime;
//struct timespec iterMPITime;

//struct timespec CpuTime;
//struct timespec MPITime;

//double iterCpuTime_avg=0, iterMPITime_avg=0 ;

// Global
int use_dpd = 0;

char prof = 0;
int policy_auto = 0;
BalancePolicy lb_funcs;


/* These flags are used to
 *  a) activate/deactive DLB functionality from the API
 *  b) protect DLB functionality before the initialization
 *  c) protect DLB_Init / DLB_MPI_Init twice
 */
static bool dlb_enabled = false;
static bool dlb_initialized = false;
static int init_id = 0;

static bool stats_enabled;
static bool drom_enabled;

static void dummyFunc() {}
static int false_dummyFunc() {return 0;}
static int true_dummyFunc() {return 1;}
static void notImplemented()
{
    warning("Functionality  Not implemented in this policy");
}

void set_dlb_enabled(bool enabled) {
    if (dlb_initialized) {
        dlb_enabled = enabled;
        if (enabled){
            lb_funcs.enableDLB();
        }else{
            lb_funcs.disableDLB();
        }
    }
}

int Initialize(void) {
    int initializer_id = 0;

    if (!dlb_initialized) {
        init_tracing();
        add_event(RUNTIME_EVENT, EVENT_INIT);

        // Set IDs
        _process_id = (_process_id == -1) ? getpid() : _process_id;
        init_id = _process_id;
        initializer_id = _process_id;

        //Read Environment vars
        char* policy;
        char* thread_distrib;
        parse_env_string_or_die( "LB_POLICY", &policy );
        parse_env_string( "LB_THREAD_DISTRIBUTION", &thread_distrib );
        parse_env_bool( "LB_JUST_BARRIER", &_just_barrier, false );
        parse_env_bool( "LB_AGGRESSIVE_INIT", &_aggressive_init, false );
        parse_env_bool( "LB_PRIORIZE_LOCALITY", &_priorize_locality, false );
        parse_env_bool( "LB_VERBOSE", &_verbose, false );
        parse_env_bool( "LB_STATISTICS", &stats_enabled, false );
        parse_env_bool( "LB_DROM", &drom_enabled, false );
        parse_env_blocking_mode( "LB_LEND_MODE", &_blocking_mode );

        if ( _aggressive_init ) {
            warning0( "FIXME: Ignoring LB_AGGRESSIVE_INIT variable. DLB cannot set the number "
                    "of threads during initialization anymore. This option will remain disabled "
                    "until we can implemented it in the runtime\n" );
            _aggressive_init = false;
        }

        if (strcasecmp(policy, "LeWI")==0) {
#ifdef debugConfig
            fprintf(stderr, "DLB: (%d:%d) - Balancing policy: LeWI\n", _node_id, _process_id);
#endif

            lb_funcs.init = &Lend_light_Init;
            lb_funcs.finish = &Lend_light_Finish;
            lb_funcs.intoCommunication = &Lend_light_IntoCommunication;
            lb_funcs.outOfCommunication = &Lend_light_OutOfCommunication;
            lb_funcs.intoBlockingCall = &Lend_light_IntoBlockingCall;
            lb_funcs.outOfBlockingCall = &Lend_light_OutOfBlockingCall;
            lb_funcs.updateresources = &Lend_light_updateresources;
            lb_funcs.returnclaimed = &dummyFunc;
            lb_funcs.releasecpu = &false_dummyFunc;
            lb_funcs.returnclaimedcpu = &false_dummyFunc;
            lb_funcs.claimcpus = &dummyFunc;
            lb_funcs.checkCpuAvailability = &true_dummyFunc;
            lb_funcs.resetDLB = &Lend_light_resetDLB;
            lb_funcs.acquirecpu = &dummyFunc;
            lb_funcs.disableDLB = &Lend_light_disableDLB;
            lb_funcs.enableDLB = &Lend_light_enableDLB;
            lb_funcs.single = &Lend_light_single;
            lb_funcs.parallel = &Lend_light_parallel;

        } else if (strcasecmp(policy, "Map")==0) {
#ifdef debugConfig
            fprintf(stderr, "DLB: (%d:%d) - Balancing policy: LeWI with Map of cpus version\n", _node_id, _process_id);
#endif

            lb_funcs.init = &Map_Init;
            lb_funcs.finish = &Map_Finish;
            lb_funcs.intoCommunication = &Map_IntoCommunication;
            lb_funcs.outOfCommunication = &Map_OutOfCommunication;
            lb_funcs.intoBlockingCall = &Map_IntoBlockingCall;
            lb_funcs.outOfBlockingCall = &Map_OutOfBlockingCall;
            lb_funcs.updateresources = &Map_updateresources;
            lb_funcs.returnclaimed = &dummyFunc;
            lb_funcs.releasecpu = &false_dummyFunc;
            lb_funcs.returnclaimedcpu = &false_dummyFunc;
            lb_funcs.claimcpus = &dummyFunc;
            lb_funcs.checkCpuAvailability = &true_dummyFunc;
            lb_funcs.resetDLB= &notImplemented;
            lb_funcs.acquirecpu = &dummyFunc;
            lb_funcs.disableDLB = &dummyFunc;
            lb_funcs.enableDLB = &dummyFunc;
            lb_funcs.single = &dummyFunc;
            lb_funcs.parallel = &dummyFunc;

        } else if (strcasecmp(policy, "WEIGHT")==0) {
#ifdef debugConfig
            fprintf(stderr, "DLB: (%d:%d) - Balancing policy: Weight balancing\n", _node_id, _process_id);
#endif
            use_dpd=1;

            lb_funcs.init = &Weight_Init;
            lb_funcs.finish = &Weight_Finish;
            lb_funcs.intoCommunication = &Weight_IntoCommunication;
            lb_funcs.outOfCommunication = &Weight_OutOfCommunication;
            lb_funcs.intoBlockingCall = &Weight_IntoBlockingCall;
            lb_funcs.outOfBlockingCall = &Weight_OutOfBlockingCall;
            lb_funcs.updateresources = &Weight_updateresources;
            lb_funcs.returnclaimed = &dummyFunc;
            lb_funcs.releasecpu = &false_dummyFunc;
            lb_funcs.returnclaimedcpu = &false_dummyFunc;
            lb_funcs.claimcpus = &dummyFunc;
            lb_funcs.checkCpuAvailability = &true_dummyFunc;
            lb_funcs.resetDLB= &notImplemented;
            lb_funcs.acquirecpu = &dummyFunc;
            lb_funcs.disableDLB = &dummyFunc;
            lb_funcs.enableDLB = &dummyFunc;
            lb_funcs.single = &dummyFunc;
            lb_funcs.parallel = &dummyFunc;

        } else if (strcasecmp(policy, "LeWI_mask")==0) {
#ifdef debugConfig
            fprintf(stderr, "DLB: (%d:%d) - Balancing policy: LeWI mask\n", _node_id, _process_id);
#endif
            lb_funcs.init = &lewi_mask_Init;
            lb_funcs.finish = &lewi_mask_Finish;
            lb_funcs.intoCommunication = &lewi_mask_IntoCommunication;
            lb_funcs.outOfCommunication = &lewi_mask_OutOfCommunication;
            lb_funcs.intoBlockingCall = &lewi_mask_IntoBlockingCall;
            lb_funcs.outOfBlockingCall = &lewi_mask_OutOfBlockingCall;
            lb_funcs.updateresources = &lewi_mask_UpdateResources;
            lb_funcs.returnclaimed = &lewi_mask_ReturnClaimedCpus;
            lb_funcs.releasecpu = &false_dummyFunc;
            lb_funcs.returnclaimedcpu = &false_dummyFunc;
            lb_funcs.claimcpus = &lewi_mask_ClaimCpus;
            lb_funcs.checkCpuAvailability = &true_dummyFunc;
            lb_funcs.resetDLB  = &lewi_mask_resetDLB;
            lb_funcs.acquirecpu = &lewi_mask_acquireCpu;
            lb_funcs.disableDLB = &lewi_mask_disableDLB;
            lb_funcs.enableDLB = &lewi_mask_enableDLB;
            lb_funcs.single = &lewi_mask_single;
            lb_funcs.parallel = &lewi_mask_parallel;

        } else if (strcasecmp(policy, "auto_LeWI_mask")==0) {
#ifdef debugConfig
            fprintf(stderr, "DLB: (%d:%d) - Balancing policy: Autonomous LeWI mask\n", _node_id, _process_id);
#endif
            lb_funcs.init = &auto_lewi_mask_Init;
            lb_funcs.finish = &auto_lewi_mask_Finish;
            lb_funcs.intoCommunication = &auto_lewi_mask_IntoCommunication;
            lb_funcs.outOfCommunication = &auto_lewi_mask_OutOfCommunication;
            lb_funcs.intoBlockingCall = &auto_lewi_mask_IntoBlockingCall;
            lb_funcs.outOfBlockingCall = &auto_lewi_mask_OutOfBlockingCall;
            lb_funcs.updateresources = &auto_lewi_mask_UpdateResources;
            //lb_funcs.returnclaimed = &auto_lewi_mask_ReturnClaimedCpus;
            lb_funcs.returnclaimed = &dummyFunc;
            lb_funcs.releasecpu = &auto_lewi_mask_ReleaseCpu;
            lb_funcs.returnclaimedcpu = &auto_lewi_mask_ReturnCpuIfClaimed;
            lb_funcs.claimcpus = &auto_lewi_mask_ClaimCpus;
            lb_funcs.checkCpuAvailability = &auto_lewi_mask_CheckCpuAvailability;
            lb_funcs.resetDLB = &auto_lewi_mask_resetDLB;
            lb_funcs.acquirecpu = &auto_lewi_mask_acquireCpu;
            lb_funcs.disableDLB = &auto_lewi_mask_disableDLB;
            lb_funcs.enableDLB = &auto_lewi_mask_enableDLB;
            lb_funcs.single = &auto_lewi_mask_single;
            lb_funcs.parallel = &auto_lewi_mask_parallel;
            policy_auto=1;

        } else if (strcasecmp(policy, "RaL")==0) {
#ifdef debugConfig
            fprintf(stderr, "DLB: (%d:%d) - Balancing policy: RaL: Redistribute and Lend \n", _node_id, _process_id);
#endif
            use_dpd=1;

            if (_mpis_per_node>1) {
                lb_funcs.init = &PERaL_Init;
                lb_funcs.finish = &PERaL_Finish;
                lb_funcs.intoCommunication = &PERaL_IntoCommunication;
                lb_funcs.outOfCommunication = &PERaL_OutOfCommunication;
                lb_funcs.intoBlockingCall = &PERaL_IntoBlockingCall;
                lb_funcs.outOfBlockingCall = &PERaL_OutOfBlockingCall;
                lb_funcs.updateresources = &PERaL_UpdateResources;
                lb_funcs.returnclaimed = &PERaL_ReturnClaimedCpus;
            } else {
                lb_funcs.init = &RaL_Init;
                lb_funcs.finish = &RaL_Finish;
                lb_funcs.intoCommunication = &RaL_IntoCommunication;
                lb_funcs.outOfCommunication = &RaL_OutOfCommunication;
                lb_funcs.intoBlockingCall = &RaL_IntoBlockingCall;
                lb_funcs.outOfBlockingCall = &RaL_OutOfBlockingCall;
                lb_funcs.updateresources = &RaL_UpdateResources;
                lb_funcs.returnclaimed = &RaL_ReturnClaimedCpus;
            }

            lb_funcs.releasecpu = &false_dummyFunc;
            lb_funcs.returnclaimedcpu = &false_dummyFunc;
            lb_funcs.claimcpus = &dummyFunc;
            lb_funcs.checkCpuAvailability = &true_dummyFunc;
            lb_funcs.resetDLB = &notImplemented;
            lb_funcs.acquirecpu = &notImplemented;
            lb_funcs.disableDLB = &dummyFunc;
            lb_funcs.enableDLB = &dummyFunc;
            lb_funcs.single = &dummyFunc;
            lb_funcs.parallel = &dummyFunc;

        } else if (strcasecmp(policy, "NO")==0) {
#ifdef debugConfig
            fprintf(stderr, "DLB: (%d:%d) - No Load balancing\n", _node_id,  _process_id);
#endif
            use_dpd=1;

            lb_funcs.init = &JustProf_Init;
            lb_funcs.finish = &JustProf_Finish;
            lb_funcs.intoCommunication = &JustProf_IntoCommunication;
            lb_funcs.outOfCommunication = &JustProf_OutOfCommunication;
            lb_funcs.intoBlockingCall = &JustProf_IntoBlockingCall;
            lb_funcs.outOfBlockingCall = &JustProf_OutOfBlockingCall;
            lb_funcs.updateresources = &JustProf_UpdateResources;
            lb_funcs.returnclaimed = &dummyFunc;
            lb_funcs.releasecpu = &false_dummyFunc;
            lb_funcs.returnclaimedcpu = &false_dummyFunc;
            lb_funcs.claimcpus = &dummyFunc;
            lb_funcs.checkCpuAvailability = &true_dummyFunc;
            lb_funcs.resetDLB = &dummyFunc;
            lb_funcs.acquirecpu = &dummyFunc;
            lb_funcs.disableDLB = &dummyFunc;
            lb_funcs.enableDLB = &dummyFunc;
            lb_funcs.single = &dummyFunc;
            lb_funcs.parallel = &dummyFunc;
        } else {
            fprintf(stderr,"DLB PANIC: Unknown policy: %s\n", policy);
            exit(1);
        }

        debug_basic_info0 ( "DLB: Balancing policy: %s balancing\n", policy);
#ifdef MPI_LIB
        debug_basic_info0 ( "DLB: MPI processes per node: %d \n", _mpis_per_node );
#endif

        pm_init();
#if 0
        if (thread_distrib==NULL) {
            if ( nanos_get_pm ) {
                const char *pm = nanos_get_pm();
                if ( strcmp( pm, "OpenMP" ) == 0 ) {
                    _default_nthreads = nanos_omp_get_max_threads();
                } else if ( strcmp( pm, "OmpSs" ) == 0 ) {
                    _default_nthreads = nanos_omp_get_num_threads();
                } else {
                    fatal0( "Unknown Programming Model\n" );
                }
            } else {
                if ( omp_get_max_threads ) {
                    _default_nthreads = omp_get_max_threads();
                } else {
                    _default_nthreads = 1;
                }
            }

            //Initial thread distribution specified
        } else {

            char* token = strtok(thread_distrib, "-");
            int i=0;
            while(i<_process_id && (token = strtok(NULL, "-"))) {
                i++;
            }

            if (i!=_process_id) {
                warning0 ("Error parsing the LB_THREAD_DISTRIBUTION (%s), using default\n");
                if ( nanos_get_pm ) {
                    const char *pm = nanos_get_pm();
                    if ( strcmp( pm, "OpenMP" ) == 0 ) {
                        _default_nthreads = nanos_omp_get_max_threads();
                    } else if ( strcmp( pm, "OmpSs" ) == 0 ) {
                        _default_nthreads = nanos_omp_get_num_threads();
                    } else {
                        fatal0( "Unknown Programming Model\n" );
                    }
                } else {
                    if ( omp_get_max_threads ) {
                        _default_nthreads = omp_get_max_threads();
                    } else {
                        _default_nthreads = 1;
                    }
                }

            } else {
                _default_nthreads=atoi(token);
            }
        }
#endif

        debug_basic_info0 ( "DLB: This process starts with %d threads\n", _default_nthreads);

        if ( _just_barrier )
            debug_basic_info0 ( "Only lending resources when MPI_Barrier "
                                "(Env. var. LB_JUST_BARRIER is set)\n" );

        if ( _blocking_mode == BLOCK )
            debug_basic_info0 ( "LEND mode set to BLOCKING. I will lend all "
                                "the resources when in an MPI call\n" );

#if IS_BGQ_MACHINE
        int bg_threadmodel;
        parse_env_int( "BG_THREADMODEL", &bg_threadmodel );
        fatal_cond0( bg_threadmodel!=2,
                     "BlueGene/Q jobs need to enable Extended thread affinity control in order to "
                     "active external threads. To do so, export BG_THREADMODEL=2\n" );
#endif

        /*  if (prof){
                clock_gettime(CLOCK_REALTIME, &initAppl);
                reset(&iterCpuTime);
                reset(&iterMPITime);
                reset(&CpuTime);
                reset(&MPITime);
                clock_gettime(CLOCK_REALTIME, &initComp);
            }*/

        /* Intercept POSIX signals to manage DLB cleanup */
        register_signals();

        lb_funcs.init();
        if ( drom_enabled ) drom_init();
        if ( stats_enabled ) stats_init();
        dlb_enabled = true;
        dlb_initialized = true;
        add_event(DLB_MODE_EVENT, EVENT_ENABLED);
        add_event(RUNTIME_EVENT, 0);
    }
    return initializer_id;
}


void Finish(int id) {
    if ( dlb_initialized && init_id == id ) {
        dlb_enabled = false;
        dlb_initialized = false;
        if ( stats_enabled ) stats_finalize();
        if ( drom_enabled ) drom_finalize();
        lb_funcs.finish();
        unregister_signals();
    }
    /*  if (prof){
            struct timespec aux, aux2;

            clock_gettime(CLOCK_REALTIME, &aux);
            diff_time(initComp, aux, &aux2);
            add_time(CpuTime, aux2, &CpuTime);

            add_time(CpuTime, iterCpuTime, &CpuTime);
            add_time(MPITime, iterMPITime, &MPITime);

            diff_time(initAppl, aux, &aux);

            if (meId==0 && _node_idId==0){
                fprintf(stdout, "DLB: Application time: %.4f\n", to_secs(aux));
                fprintf(stdout, "DLB: Iterations detected: %d\n", iterNum);
            }
            fprintf(stdout, "DLB: (%d:%d) - CPU time: %.4f\n", _node_idId, meId, to_secs(CpuTime));
            fprintf(stdout, "DLB: (%d:%d) - MPI time: %.4f\n", _node_idId, meId, to_secs(MPITime));
        }*/
}

void Terminate(void) {
    if ( stats_enabled ) stats_finalize();
    if ( drom_enabled ) drom_finalize();
    lb_funcs.finish();
}

void Update() {
    if ( drom_enabled ) drom_update();
    if ( stats_enabled ) stats_update();
}

void IntoCommunication(void) {
    /*  struct timespec aux;
        clock_gettime(CLOCK_REALTIME, &initMPI);
        diff_time(initComp, initMPI, &aux);
        add_time(iterCpuTime, aux, &iterCpuTime);*/

    if (dlb_enabled) {
        lb_funcs.intoCommunication();
    }
}

void OutOfCommunication(void) {
    /*  struct timespec aux;
        clock_gettime(CLOCK_REALTIME, &initComp);
        diff_time(initMPI, initComp, &aux);
        add_time(iterMPITime, aux, &iterMPITime);*/

    if (dlb_enabled) {
        lb_funcs.outOfCommunication();
    }
}

void IntoBlockingCall(int is_iter, int blocking_mode) {
    /*  double cpuSecs;
        double MPISecs;
        cpuSecs=iterCpuTime_avg;
        MPISecs=iterMPITime_avg;*/
    if (dlb_enabled) {
        // We explicitly ignore the argument
        lb_funcs.intoBlockingCall(is_iter, _blocking_mode );
    }
}

void OutOfBlockingCall(int is_iter) {
    if (dlb_enabled) {
        lb_funcs.outOfBlockingCall(is_iter);
    }
}

void updateresources( int max_resources ) {
    if (dlb_enabled) {
        add_event(RUNTIME_EVENT, EVENT_UPDATE);
        lb_funcs.updateresources( max_resources );
        add_event(RUNTIME_EVENT, EVENT_USER);
    }
}

void returnclaimed( void ) {
    if (dlb_enabled) {
        add_event(RUNTIME_EVENT, EVENT_RETURN);
        lb_funcs.returnclaimed();
        add_event(RUNTIME_EVENT, EVENT_USER);
    }
}

int releasecpu( int cpu ) {
    if (dlb_enabled) {
        return lb_funcs.releasecpu(cpu);
    } else {
        return 0;
    }
}

int returnclaimedcpu( int cpu ) {
    if (dlb_enabled) {
        return lb_funcs.returnclaimedcpu(cpu);
    } else {
        return 0;
    }
}

void claimcpus( int cpus ) {
    if (dlb_enabled) {
        add_event(RUNTIME_EVENT, EVENT_CLAIM_CPUS);
        lb_funcs.claimcpus(cpus);
        add_event(RUNTIME_EVENT, EVENT_USER);
    }
}

void acquirecpu (int cpu){
    if (dlb_enabled) {
        add_event(RUNTIME_EVENT, EVENT_ACQUIRE_CPU);
        lb_funcs.acquirecpu(cpu);
        add_event(RUNTIME_EVENT, EVENT_USER);
    }
}

int checkCpuAvailability (int cpu) {
    if (dlb_enabled) {
        return lb_funcs.checkCpuAvailability(cpu);
    } else {
        return 1;
    }
}

void resetDLB () {
    if (dlb_enabled) {
        lb_funcs.resetDLB();
    }
}

void singlemode () {
    dlb_enabled = false;
    lb_funcs.single();
}

void parallelmode () {
    dlb_enabled = true;
    lb_funcs.parallel();
}

int is_auto( void ){
   return policy_auto;
}
