#!/usr/bin/env bash
# Builds the trailer camera (tests/film/trailer_film.cpp) for the host.
set -euo pipefail
cd "$(dirname "$0")/../.."
SRC=app/src/main/cpp
ENGINE=$(sed -n 's/^ENGINE=(\(.*\))$/\1/p' tests/run_tests.sh)
eval "ENGINE=($ENGINE)"
mkdir -p build/tests
g++ -std=c++17 -O2 -g0 -I$SRC -isystem build/tests/include tests/film/trailer_film.cpp tests/film/renderer_film.cpp \
    tests/png_writer.cpp "${ENGINE[@]}" -o build/tests/trailer_film -lz
