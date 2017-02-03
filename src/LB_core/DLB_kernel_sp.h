#ifndef DLB_KERNEL_SP_H
#define DLB_KERNEL_SP_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <sched.h>
#include <stdbool.h>

#include "LB_core/spd.h"
#include "LB_core/dlb_types.h"
#include "support/options.h"

/* Status */
subprocess_descriptor_t* Initialize_sp(const cpu_set_t *mask, const char *lb_args);
int Finish_sp(subprocess_descriptor_t *spd);
int set_dlb_enabled_sp(subprocess_descriptor_t *psd, bool enabled);
int set_max_parallelism_sp(subprocess_descriptor_t *psd, int max);

/* Callbacks */
int callback_set_sp(subprocess_descriptor_t *psd, dlb_callbacks_t which, dlb_callback_t callback);
int callback_get_sp(subprocess_descriptor_t *psd, dlb_callbacks_t which, dlb_callback_t callback);

/* Lend */
int lend_sp(subprocess_descriptor_t *psd);
int lend_cpu_sp(subprocess_descriptor_t *psd, int cpuid);
int lend_cpus_sp(subprocess_descriptor_t *psd, int ncpus);
int lend_cpu_mask_sp(subprocess_descriptor_t *psd, const cpu_set_t *mask);

/* Reclaim */
int reclaim_sp(subprocess_descriptor_t *psd);
int reclaim_cpu_sp(subprocess_descriptor_t *psd, int cpuid);
int reclaim_cpus_sp(subprocess_descriptor_t *psd, int ncpus);
int reclaim_cpu_mask_sp(subprocess_descriptor_t *psd, const cpu_set_t *mask);

/* Acquire */
int acquire_sp(subprocess_descriptor_t *psd);
int acquire_cpu_sp(subprocess_descriptor_t *psd, int cpuid);
int acquire_cpus_sp(subprocess_descriptor_t *psd, int ncpus);
int acquire_cpu_mask_sp(subprocess_descriptor_t *psd, const cpu_set_t* mask);

/* Return */
int return_all_sp(subprocess_descriptor_t *psd);
int return_cpu_sp(subprocess_descriptor_t *psd, int cpuid);

/* DROM Responsive */
int poll_drom_sp(subprocess_descriptor_t *psd, int *new_threads, cpu_set_t *new_mask);

/* Misc */
int set_variable_sp(subprocess_descriptor_t *psd, const char *variable, const char *value);
int get_variable_sp(subprocess_descriptor_t *psd, const char *variable, char *value);
int print_variables_sp(subprocess_descriptor_t *psd);

#endif /* DLB_KERNEL_SP_H */
