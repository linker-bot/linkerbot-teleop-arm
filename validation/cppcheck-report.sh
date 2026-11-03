#!/usr/bin/env bash
set -euo pipefail
# Retain real project/header diagnostics. Cppcheck models standard-library
# system headers; clang-tidy independently validates the complete include graph.
exec cppcheck --library=std --library=googletest --suppress=missingIncludeSystem "$@"
