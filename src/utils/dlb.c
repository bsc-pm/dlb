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

/*! \page dlb DLB utility command.
 *  \section synopsis SYNOPSIS
 *      <B>dlb</B> {--affinity | --gpu-affinity [--uuid] | --test-init |
 *                  --help [--help] | --version}
 *  \section description DESCRIPTION
 *      Utility command to display all DLB options, print DLB version, or check
 *      process affinity.
 *
 *      <DL>
 *          <DT>-a, --affinity</DT>
 *          <DD>Print process affinity.</DD>
 *
 *          <DT>-g, --gpu-affinity</DT>
 *          <DD>Print process and GPU affinity.</DD>
 *
 *          <DT>-u, --uuid</DT>
 *          <DD>Show full UUID with GPU affinity.</DD>
 *
 *          <DT>-i, --test-init</DT>
 *          <DD>Test DLB initialization.</DD>
 *
 *          <DT>-h, --help</DT>
 *          <DD>Print DLB variables and current value. Use the option twice
 *          (e.g., -hh) for extended info.</DD>
 *
 *          <DT>-v, --version</DT>
 *          <DD>Print version info.</DD>
 *      </DL>
 *  \section author AUTHOR
 *      Barcelona Supercomputing Center (dlb@bsc.es)
 *  \section seealso SEE ALSO
 *      \ref dlb_run "dlb_run"(1), \ref dlb_shm "dlb_shm"(1),
 *      \ref dlb_taskset "dlb_taskset"(1)
 */

#include "utils/dlb_util.h"

int main(int argc, char *argv[]) {

    return dlb_util(argc, argv);
}
