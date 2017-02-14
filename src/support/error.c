/*********************************************************************************/
/*  Copyright 2017 Barcelona Supercomputing Center                               */
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

#include "support/error.h"

#include "apis/dlb_errors.h"

static const char* error_msg[] = {
    /* DLB_NOUPDT */        "no update needed",
    /* DLB_NOTED */         "resource not available, but petition attended",
    /* DLB_SUCCESS */       "Success",
    /* DLB_ERR_UNKNOWN */   "Unknown error",
    /* DLB_ERR_NOINIT */    "DLB is not initialized",
    /* DLB_ERR_INIT */      "DLB already initialized",
    /* DLB_ERR_DISBLD */    "DLB is disabled",
    /* DLB_ERR_NOSHMEM */   "No shared memory",
    /* DLB_ERR_NOPROC */    "No pid",
    /* DLB_ERR_PDIRTY */    "pid dirty, can't update",
    /* DLB_ERR_PERM */      "permission error",
    /* DLB_ERR_TIMEOUT */   "timeout",
    /* DLB_ERR_NOCBK */     "no callback defined",
    /* DLB_ERR_NOENT */     "no entry",
    /* DLB_ERR_NOCOMP */    "no compatible",
    /* DLB_ERR_REQST */     "too many requests",
    /* DLB_ERR_NOMEM */     "not enough space"
};

const char* error_get_str(int errnum) {
    if (errnum >= _DLB_ERROR_UPPER_BOUND || errnum <= _DLB_ERROR_LOWER_BOUND) {
        return "unknown errnum";
    }
    return error_msg[-errnum + _DLB_ERROR_UPPER_BOUND - 1];
}
