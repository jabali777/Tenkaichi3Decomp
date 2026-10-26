#!/bin/sh
# Pruebas sin ventana, GPU, port instalado ni datos del juego.
set -eu
root=$(CDPATH= cd "$(dirname "$0")/.." && pwd -P)
cd "$root"
work=$(mktemp -d)
trap 'rm -rf -- "$work"' 0
trap 'exit 1' HUP INT TERM
for script in play.sh play-fast.sh play-diagnostics.sh optimization/build.sh scripts/check.sh; do
    sh -n "$script"
done
for test in uniforms resolution; do
    "${CC:-cc}" -O2 -std=c11 -Wall -Wextra -Werror \
        "optimization/test_$test.c" -o "$work/test_$test" -ldl
    "$work/test_$test"
done
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s optimization -p 'test_*.py' -v
printf '%s\n' 'Verification complete.'
