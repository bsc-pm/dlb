/*********************************************************************************/
/*  Copyright 2009-2026 Barcelona Supercomputing Center                          */
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

#ifndef SMALL_ARRAY_H
#define SMALL_ARRAY_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "support/queues.h"
#include "support/types.h"


#define SMALL_ARRAY_TYPE int
#include "support/small_array_template.h"


#define SMALL_ARRAY_TYPE int64_t
#include "support/small_array_template.h"


#define SMALL_ARRAY_TYPE pid_t
#include "support/small_array_template.h"


#define SMALL_ARRAY_TYPE cpuid_t
#include "support/small_array_template.h"


#define SMALL_ARRAY_TYPE lewi_request_t
#include "support/small_array_template.h"


#define SMALL_ARRAY(type, var_name, size) \
    small_array_##type##_t __##var_name __attribute__ ((__cleanup__(small_array_##type##_free))); \
    type *var_name = small_array_##type##_init(&__##var_name, size);


#endif /* SMALL_ARRAY_H */
