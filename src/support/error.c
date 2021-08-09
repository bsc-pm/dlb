/*********************************************************************************/
/*  Copyright 2009-2021 Barcelona Supercomputing Center                          */
/*                                                                               */
/*  This file is part of the DLB library.                                        */
/*                                                                               */
/*  DLB is free software: you can redistribute it and/or modify                  */
/*  it under the terms of the GNU Lesser General Public License as published by  */
/*  the Free Software Foundation, either version 3 of the License, or            */
/*  (at your option) any later version.                                          */
/*                                                                               */
/*  DLB is distributed in the hope that it will be useful,                       */
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of               */
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                */
/*  GNU Lesser General Public License for more details.                          */
/*                                                                               */
/*  You should have received a copy of the GNU Lesser General Public License     */
/*  along with DLB.  If not, see <https://www.gnu.org/licenses/>.                */
/*********************************************************************************/

#include "support/error.h"

#include "apis/dlb_errors.h"

static const char* error_msg[] = {
    "DLB_NOUPDT (2): The requested operation does not need any action",
    "DLB_NOTED (1): The operation cannot be performed now, but it has been attended",
    "DLB_SUCCESS (0): Success",
    "DLB_ERR_UNKNOWN (-1): Unknown error",
    "DLB_ERR_NOINIT (-2): DLB has not been initialized",
    "DLB_ERR_INIT (-3): DLB is already initialized",
    "DLB_ERR_DISBLD (-4): DLB is disabled",
    "DLB_ERR_NOSHMEM (-5): DLB cannot find a shared memory",
    "DLB_ERR_NOPROC (-6): DLB cannot find the requested process",
    "DLB_ERR_PDIRTY (-7): DLB cannot update the target process, another operation still in process",
    "DLB_ERR_PERM (-8): DLB cannot acquire the requested resource",
    "DLB_ERR_TIMEOUT (-9): The operation has timed out",
    "DLB_ERR_NOCBK (-10): The callback is not defined and cannot be invoked",
    "DLB_ERR_NOENT (-11): The entry does not exist",
    "DLB_ERR_NOCOMP (-12): The operation is not compatible with the configured DLB options",
    "DLB_ERR_REQST (-13): DLB cannot take more requests for a specific resource",
    "DLB_ERR_NOMEM (-14): DLB cannot allocate more processes into the shared memory",
    "DLB_ERR_NOPOL (-15): The operation is not defined in the current policy",
    "DLB_ERR_NOTALP (-16): TALP is not initialized"
};

const char* error_get_str(int errnum) {
    if (errnum >= _DLB_ERROR_UPPER_BOUND || errnum <= _DLB_ERROR_LOWER_BOUND) {
        return "unknown errnum";
    }
    return error_msg[-errnum + _DLB_ERROR_UPPER_BOUND - 1];
}
