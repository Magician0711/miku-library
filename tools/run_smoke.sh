#!/usr/bin/env bash
# Miku Library - Copyright (C) 2026 Magician0711
# SPDX-License-Identifier: GPL-3.0-only
#
# Runs the nine smoke suites on macOS or Linux and prints PASS/FAIL counts from their logs.
#   tools/run_smoke.sh <build directory>
# On a Linux machine without a display, run it under xvfb-run. Exits non-zero on any failure.
set -u
build=${1:-build}
cd "$build" || exit 2
failed=()
for test in DetailLibrarySmoke DirectorySmoke DownloadSmoke ExternalDownloaderSmoke LocalLibrarySmoke \
            OptimizationSmoke QuickUiSmoke RpcSmoke SettingsLimitsSmoke; do
    log=$(echo "$test" | sed -E 's/Smoke$//; s/([a-z])([A-Z])/\1-\2/g' | tr 'A-Z' 'a-z')-smoke.log
    rm -f "$log"
    if command -v timeout >/dev/null; then
        timeout 600 "./$test" --test >/dev/null 2>&1
    else
        "./$test" --test >/dev/null 2>&1
    fi
    code=$?
    passed=0; failures=0
    if [ -f "$log" ]; then
        passed=$(grep -c ' PASS$' "$log")
        failures=$(grep -c ' FAIL$' "$log")
    fi
    echo "$test exit=$code passed=$passed failed=$failures"
    if [ "$code" -ne 0 ]; then
        failed+=("$test")
        if [ -f "$log" ]; then
            # In GitHub Actions each failure becomes a notice, readable without the log.
            if [ -n "${GITHUB_ACTIONS:-}" ]; then
                grep ' FAIL$' "$log" | while IFS= read -r line; do echo "::error title=$test::$line"; done
            else
                grep ' FAIL$' "$log" | sed 's/^/  /'
            fi
        fi
    fi
done
if [ ${#failed[@]} -gt 0 ]; then
    echo "Failed: ${failed[*]}"
    exit 1
fi
