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

#ifndef ENV_H
#define ENV_H

typedef enum env_add_condition_e {
    ENV_OVERWRITE_NEVER,        /* variable is set only if name does not exist */
    ENV_OVERWRITE_ALWAYS,       /* variable is set even if name exists */
    ENV_UPDATE_IF_EXISTS,       /* variable is set only if name exists */
    ENV_APPEND                  /* variable is updated appending value */
} env_add_condition_t;

void dlb_setenv(const char *name, const char *value, char ***next_environ,
        env_add_condition_t condition);

#endif /* ENV_H */
