#!/bin/sh

dlb_git_branch="$1"
dlb_version="$2"

echo "$dlb_git_branch" > "$MESON_DIST_ROOT"/.tarball-git-branch
echo "$dlb_version" > "$MESON_DIST_ROOT"/.tarball-version
