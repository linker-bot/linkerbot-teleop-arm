#!/usr/bin/env bash
set -euo pipefail
# Retain real project/header diagnostics. Cppcheck models standard-library
# system headers; clang-tidy independently validates the complete include graph.
script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
log_dir="${GITHUB_WORKSPACE:-$script_dir/..}/build/report-cppcheck"
mkdir -p "$log_dir"
log_file="$(mktemp "$log_dir/stderr.XXXXXX.xml")"
status=0
# Preserve unfiltered XML, including errors in dependency headers which the
# report formatter may omit by path while still marking analysis incomplete.
cppcheck --library="$script_dir/googletest-cppcheck.cfg" --library=std --library=googletest \
    --suppress=missingIncludeSystem "$@" 2> >(tee "$log_file" >&2) || status=$?
wait
exit "$status"
