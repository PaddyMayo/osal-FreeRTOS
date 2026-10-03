#!/usr/bin/env bash
# Builds and runs the freertos white-box coverage tests, then checks gcov
# coverage of freertos/src/ against the gate in gcovr.cfg.
#
# Runs as the freertos-coverage pre-commit hook (and so in CI's pre-check
# job), but can be run directly. Set BUILD_DIR to use a build directory
# other than build/.
#
# Stale .gcda counters from earlier runs are wiped first, so the report
# only reflects the tests as they are now.
set -euo pipefail

cd "$(git rev-parse --show-toplevel)"
build_dir="${BUILD_DIR:-build}"

if ! command -v gcovr >/dev/null 2>&1; then
  echo "gcovr is not installed - rebuild the devcontainer (or pip3 install gcovr)." >&2
  exit 1
fi

cmake -S . -B "$build_dir" >/dev/null
cmake --build "$build_dir" --target freertos_coverage_tests -j
find "$build_dir/freertos-coverage-tests" -name '*.gcda' -delete
ctest --test-dir "$build_dir" -R '^coverage-freertos-' --output-on-failure
gcovr "$build_dir/freertos-coverage-tests"
