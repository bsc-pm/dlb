#ifndef DLB_DROM_H
#define DLB_DROM_H

#include "dlb_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

/*********************************************************************************/
/*    Dynamic Resource Manager Module                                            */
/*********************************************************************************/

/* \brief Initialize DROM Module
 * \return error code
 */
int DLB_DROM_Init(void);

/* \brief Finalize DROM Module
 * \return error code
 */
int DLB_DROM_Finalize(void);

/* \brief Get the total number of available CPUs in the node
 * \param[out] nthreads the number of CPUs
 * \return error code
 */
int DLB_DROM_GetNumCpus(int *nthreads);

/* \brief Get the PID's attached to this module
 * \param[out] pidlist The output list
 * \param[out] nelems Number of elements in the list
 * \param[in] max_len Max capacity of the list
 * \return error code
 */
int DLB_DROM_GetPidList(int *pidlist, int *nelems, int max_len);

/* \brief Get the process mask of the given PID
 * \param[in] pid Process ID to consult
 * \param[out] mask Current process mask
 * \return error code
 */
int DLB_DROM_GetProcessMask(int pid, dlb_cpu_set_t mask);

/* \brief Set the process mask of the given PID
 * \param[in] pid Process ID to signal
 * \param[in] mask Process mask to set
 * \return error code
 */
int DLB_DROM_SetProcessMask(int pid, const_dlb_cpu_set_t mask);

/* \brief Ask for free (or stolen) CPUs in the node
 * \param[in] cpus Number of CPUs to get
 * \param[in] steal Whether if look only into free CPUs or force stealing
 * \param[out] cpulist The output list
 * \param[out] nelems Numer of element in the list
 * \param[in] max_len Max capacity of the list
 * \return error code
 */
int DLB_DROM_GetCpus(int ncpus, int steal, int *cpulist, int *nelems, int max_len);

/* \brief Register PID with the given mask before the process normal registration
 * \param[in] pid Process ID that gets the reservation
 * \param[in] mask Process mask to register
 * \param[in] steal whether to steal owned CPUs
 * \return error code
 */
int DLB_DROM_PreRegister(int pid, const_dlb_cpu_set_t mask, int steal);

#ifdef __cplusplus
}
#endif

#endif /* DLB_DROM_H */
