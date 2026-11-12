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

// This file contains portions derived from EAR library.
// Original copyright:
/***************************************************************************
 * Copyright (c) 2024 Energy Aware Runtime - Barcelona Supercomputing Center
 *
 * This program and the accompanying materials are made
 * available under the terms of the Eclipse Public License 2.0
 * which is available at https://www.eclipse.org/legal/epl-2.0/
 *
 * SPDX-License-Identifier: EPL-2.0
 **************************************************************************/

#ifndef STATES_H
#define STATES_H

#define EAR_SUCCESS         0
#define EAR_ERROR           -1
#define EAR_WARNING         -2
#define EAR_NOT_INITIALIZED -8
#define EAR_BAD_ARGUMENT    -13
#define EAR_TIMEOUT         -21
#define EAR_UNDEFINED       -24
#define EAR_CANCELLED       -25

typedef int state_t;

/* global data */
char *state_msg __attribute__((weak));

#define state_ok(state)          ((state) == EAR_SUCCESS)

#define state_fail(state)        ((state) != EAR_SUCCESS)

#endif // STATES_H
