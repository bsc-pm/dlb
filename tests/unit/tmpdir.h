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

#ifndef TMPDIR_H
#define TMPDIR_H

#include <ftw.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

static char test_tmpdir[PATH_MAX];

static int tmpdir_remove_cb(const char *fpath, const struct stat *sb,
        int typeflag, struct FTW *ftwbuf) {
    (void)sb;
    (void)typeflag;
    (void)ftwbuf;
    return remove(fpath);
}

static void tmpdir_cleanup(void) {
    if (test_tmpdir[0] != '\0') {
        nftw(test_tmpdir, tmpdir_remove_cb, 16, FTW_DEPTH | FTW_MOUNT | FTW_PHYS);
        test_tmpdir[0] = '\0';
    }
}

/* Creates the directory on first call, returns its path or fail.
 * Cleanup is registered via atexit. */
static const char *tmpdir_get(void) {
    if (test_tmpdir[0] == '\0') {
        const char *base = getenv("TMPDIR");
        int n = snprintf(test_tmpdir, sizeof(test_tmpdir),
                         "%s/dlb_test.XXXXXX", base ? base : "/tmp");
        if (n < 0
                || (size_t)n >= sizeof(test_tmpdir)
                || !mkdtemp(test_tmpdir)) {
            perror("tmpdir_get");
            test_tmpdir[0] = '\0';
            abort();
        }
        atexit(tmpdir_cleanup);
    }
    return test_tmpdir;
}

/* Convenience: build a path inside the tmpdir. */
static int tmpdir_path(char *out, size_t outsz, const char *name) {
    return snprintf(out, outsz, "%s/%s", tmpdir_get(), name);
}

#endif /* TMPDIR_H */
