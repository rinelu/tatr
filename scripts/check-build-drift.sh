#!/usr/bin/env bash

# The Makefile and CMakeLists.txt both read their non-module source
# lists from sources/*.txt, so in the normal case they can't drift.
# This script asks each build system to resolve its 'own' full
# source list (including the glob-based module sources) and diffs the
# result, so a manifest that only one of the two files actually reads,
# or a hardcoded source slipped back into one of them by hand, still
# gets caught.

set -euo pipefail

cd "$(dirname "$0")/.."

make_list="$(mktemp)"
cmake_list="$(mktemp)"
cmake_build_dir="$(mktemp -d)"
trap 'rm -rf "$make_list" "$cmake_list" "$cmake_build_dir"' EXIT

make print-sources > "$make_list"

cmake -S . -B "$cmake_build_dir" -DCMAKE_BUILD_TYPE=Debug > /dev/null
sort -o "$cmake_list" "$cmake_build_dir/cmake-sources.txt"
sort -o "$make_list" "$make_list"

if ! diff -u "$cmake_list" "$make_list" > /tmp/build-drift.diff; then
    echo "Makefile and CMakeLists.txt disagree on the set of source files:"
    echo
    cat /tmp/build-drift.diff
    echo
    echo "(< is CMakeLists.txt, > is Makefile)"
    echo
    echo "If you added/removed an unconditional source file, edit the"
    echo "matching file under sources/ instead of Makefile/CMakeLists.txt"
    echo "directly. If you added a new module, both build files need the"
    echo "same module-registration edit (see sources/README.md)."
    exit 1
fi

echo "OK: Makefile and CMakeLists.txt agree on $(wc -l < "$make_list") source files."
