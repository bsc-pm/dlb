#ifndef DLB_SP_H
#define DLB_SP_H

#include "dlb_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

/*********************************************************************************/
/*    Status                                                                     */
/*********************************************************************************/

/* \brief Initialize DLB library
 * \param[in] mask
 * \param[in] dlb_args
 * \return opaque dlb handler
 */
dlb_handler_t DLB_Init_sp(const_dlb_cpu_set_t mask, const char *dlb_args);

/* \brief Finalize DLB library
 * \param[in] handler
 * \return
 */
int DLB_Finalize_sp(dlb_handler_t handler);

/* \brief Enable DLB
 * \param[in] handler
 * \return
 *
 * Only if the library was previously disabled, subsequent DLB calls will be
 * attended again.
 */
int DLB_Enable_sp(dlb_handler_t handler);

/* \brief Disable DLB
 * \param[in] handler
 * \return
 *
 * The process will recover the original status of its resources, and further DLB
 * calls will be ignored.
 */
int DLB_Disable_sp(dlb_handler_t handler);

/* \brief DLB will avoid to assign more resources that the maximum provided
 * \param[in] handler
 * \param[in] max
 * \return
 */
int DLB_SetMaxParallelism_sp(dlb_handler_t handler, int max);


/*********************************************************************************/
/*    Callbacks                                                                  */
/*********************************************************************************/

/* \brief Set callback
 * \param[in] handler
 * \param[in] which
 * \param[in] callback
 * \return
 */
int DLB_CallbackSet_sp(dlb_handler_t handler, dlb_callbacks_t which, dlb_callback_t callback);

/* \brief Set callback
 * \param[in] handler
 * \param[in] which
 * \param[out] callback
 * \return
 */
int DLB_CallbackGet_sp(dlb_handler_t handler, dlb_callbacks_t which, dlb_callback_t callback);


/*********************************************************************************/
/*    Lend                                                                       */
/*********************************************************************************/

/* \brief Lend all the current CPUs
 * \param[in] handler
 * \return
 */
int DLB_Lend_sp(dlb_handler_t handler);


/* \brief Lend a specific CPU
 * \param[in] handler
 * \param[in] cpuid
 * \return
 */
int DLB_LendCpu_sp(dlb_handler_t handler, int cpuid);

/* \brief
 * \param[in] handler
 * \param[in] ncpus
 * \return
 */
int DLB_LendCpus_sp(dlb_handler_t handler, int ncpus);

/* \brief
 * \param[in] handler
 * \param[in] mask
 * \return
 */
int DLB_LendCpuMask_sp(dlb_handler_t handler, const_dlb_cpu_set_t mask);


/*********************************************************************************/
/*    Reclaim                                                                    */
/*********************************************************************************/

/* \brief Retrieve the resources owned by the process
 * \param[in] handler
 * \return
 *
 * Retrieve the CPUs owned by the process that were previously lent to DLB.
 */
int DLB_Reclaim_sp(dlb_handler_t handler);

/* \brief Retrieve the resources owned by the process
 * \param[in] handler
 * \param[in] cpuid
 * \return
 */
int DLB_ReclaimCpu_sp(dlb_handler_t handler, int cpuid);

/* \brief Claim some of the CPUs owned by this process
 * \param[in] handler
 * \param[in] ncpus Number of CPUs to reclaim
 * \return
 *
 * Claim up to 'cpus' CPUs of which this process is the owner
 */
int DLB_ReclaimCpus_sp(dlb_handler_t handler, int ncpus);

/* \brief
 * \param[in] handler
 * \param[in] mask
 * \return
 */
int DLB_ReclaimCpuMask_sp(dlb_handler_t handler, const_dlb_cpu_set_t mask);


/*********************************************************************************/
/*    Acquire                                                                    */
/*********************************************************************************/

/* \brief
 * \param[in] handler
 * \return
 */
int DLB_Acquire_sp(dlb_handler_t handler);

/* \brief Try to acquire a specific CPU
 * \param[in] handler
 * \param[in] cpu CPU to acquire
 * \return
 *
 * Whenever is possible, DLB will assign the specified CPU to the current process
 */
int DLB_AcquireCpu_sp(dlb_handler_t handler, int cpuid);

/* \brief
 * \param[in] handler
 * \param[in] ncpus
 * \return
 */
int DLB_AcquireCpus_sp(dlb_handler_t handler, int ncpus);

/* \brief Try to acquire a set of CPUs
 * \param[in] handler
 * \param[in] mask CPU set to acquire
 * \return
 *
 * Whenever is possible, DLB will assign the specified CPUs to the current process
 */
int DLB_AcquireCpuMask_sp(dlb_handler_t handler, const_dlb_cpu_set_t mask);


/*********************************************************************************/
/*    Return                                                                     */
/*********************************************************************************/

/* \brief Return claimed CPUs of other processes
 * \param[in] handler
 * \return
 *
 * Return those external CPUs that are currently assigned to me and have been claimed
 * by their owner.
 */
int DLB_Return_sp(dlb_handler_t handler);

/* \brief Return the specific CPU if it has been reclaimed
 * \param[in] handler
 * \param[in] cpuid CPU to return
 * \return
 *
 * Return the specific CPU if it is currently assigned to me and has been claimed
 * by its owner.
 */
int DLB_ReturnCpu_sp(dlb_handler_t handler, int cpuid);


/*********************************************************************************/
/*    DROM Responsive                                                            */
/*********************************************************************************/

/* \brief
 * \param[in] handler
 * \param[out] nthreads
 * \param[out] mask
 * \return
 */
int DLB_PollDROM_sp(dlb_handler_t handler, int *nthreads, dlb_cpu_set_t mask);


/*********************************************************************************/
/*    Misc                                                                       */
/*********************************************************************************/

/* \brief Set DLB internal variable
 * \param[in] handler
 * \param[in] variable Internal variable to set
 * \param[in] value New value
 * \return
 *
 * Change the value of a DLB internal variable
 */
int DLB_SetVariable_sp(dlb_handler_t handler, const char *variable, const char *value);

/* \brief Get DLB internal variable
 * \param[in] handler
 * \param[in] variable Internal variable to set
 * \param[out] value Current DLB variable value
 * \return
 *
 * Query the value of a DLB internal variable
 */
int DLB_GetVariable_sp(dlb_handler_t handler, const char *variable, char *value);

/* \brief Print DLB internal variables
 * \param[in] handler
 * \return
 */
int DLB_PrintVariables_sp(dlb_handler_t handler);


#ifdef __cplusplus
}
#endif

#endif /* DLB_SP_H */
