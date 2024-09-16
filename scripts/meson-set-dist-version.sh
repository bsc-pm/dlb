#!/bin/sh

cd "$MESON_SOURCE_ROOT" || return
./scripts/git-version-gen > "$MESON_DIST_ROOT"/.tarball-version
