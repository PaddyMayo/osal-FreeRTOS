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
#
# gcovr only sees files some test executable compiled, so a freertos/src/
# file with no white-box test (no freertos-coverage-tests/src/
# coveragetest-<module>.c) would silently drop out of the gate. Every
# freertos/src/*.c must therefore also appear in the report.
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

summary="$build_dir/freertos-coverage-summary.json"
gcovr "$build_dir/freertos-coverage-tests" --json-summary "$summary"

missing="$(python3 - "$summary" <<'PY'
import glob, json, sys
covered = {f["filename"] for f in json.load(open(sys.argv[1]))["files"]}
for src in sorted(glob.glob("freertos/src/*.c")):
    if src not in covered:
        print(src)
PY
)"
if [ -n "$missing" ]; then
  echo "No white-box coverage test exercises these freertos/src/ files:" >&2
  sed 's/^/  /' <<<"$missing" >&2
  echo "Add freertos-coverage-tests/src/coveragetest-<module>.c for each." >&2
  exit 1
fi
