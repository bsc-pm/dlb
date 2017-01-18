/*********************************************************************************/
/*  Copyright 2016 Barcelona Supercomputing Center                               */
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
/*  along with DLB.  If not, see <http://www.gnu.org/licenses/>.                 */
/*********************************************************************************/

#include <stdio.h>

#include "error.h"

static const char* error_msg[] = {
    /* DLB_SUCCESS */       "Success",
    /* DLB_ERR_UNKNOWN */   "Unknown error",
    /* DLB_ERR_NOINIT */    "DLB is not initialized",
    /* DLB_ERR_DISBLD */    "DLB is disabled",
    /* DLB_ERR_NOSHMEM */   "No shared memory",
    /* DLB_ERR_NOPROC */    "No pid",
    /* DLB_ERR_PDIRTY */    "pid dirty, can't update",
    /* DLB_ERR_PERM */      "permission error",
    /* DLB_ERR_TIMEOUT */   "timeout",
    /* DLB_ERR_NOUPDT */    "no update needed"
};

const char* error_get_str(int errnum) {
    if (errnum > 0 || errnum < DLB_MAX_ERRORS) {
        return "unknown errnum";
    }
    return error_msg[-errnum];
}

// Thread safe reentrant needed?
// int error_get_str_r(int errnum, char *buf, size_t buflen)
// {
// }
