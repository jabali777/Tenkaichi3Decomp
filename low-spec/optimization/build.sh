#!/bin/sh
set -eu
cd "$(dirname "$0")"
if ! command -v "${CC:-cc}" >/dev/null 2>&1; then
    printf '%s\n' 'C compiler not found. Install GCC or Clang (CC=clang).' >&2
    exit 1
fi
build_tmp=$(mktemp ./libbt3_opengl_fast.XXXXXX.so)
lowres_tmp=$(mktemp ./libbt3_resolution.XXXXXX.so)
trap 'rm -f -- "$build_tmp" "$lowres_tmp"' 0
trap 'exit 1' HUP INT TERM
"${CC:-cc}" -O2 -shared -fPIC -std=c11 -Wall -Wextra -Werror \
    -o "$build_tmp" bt3_opengl_fast.c -ldl
"${CC:-cc}" -O2 -shared -fPIC -std=c11 -Wall -Wextra -Werror \
    -o "$lowres_tmp" bt3_reduced_resolution.c -ldl
chmod 644 "$build_tmp" "$lowres_tmp"
mv -f -- "$build_tmp" libbt3_opengl_fast.so
mv -f -- "$lowres_tmp" libbt3_resolution.so
