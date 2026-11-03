#!/usr/bin/env bash
set -euo pipefail
root=$(cd -- "$(dirname -- "$0")" && pwd)
args=(-DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON)
verbose=()
coverage=false
for arg in "$@"; do
  case "$arg" in
    -v|--verbose) verbose=(--verbose) ;;
    -c|--coverage) coverage=true ;;
    -h|--help) echo "Usage: $0 [--verbose] [--coverage]"; exit 0 ;;
    *) echo "Unknown option: $arg" >&2; exit 2 ;;
  esac
done
cmake -S "$root/src/linkerta" -B "$root/build_test" "${args[@]}"
cmake --build "$root/build_test" --parallel "${verbose[@]}"
ctest --test-dir "$root/build_test" --output-on-failure --no-tests=error
if $coverage; then
  cmake --build "$root/build_test" --target coverage
fi
