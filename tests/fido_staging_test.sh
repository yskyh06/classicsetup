#!/bin/sh

set -eu

project_root=$1
expected_hash=cfd466e79ae8c687a09447249534a99903ed5c52ce3b0cc4fb71475058ab66f7
fido_script=$project_root/tools/fido/fido-linux.ps1

test -f "$fido_script"
actual_hash=$(sha256sum "$fido_script" | awk '{print $1}')
test "$actual_hash" = "$expected_hash"
grep -q "^tag=v1.70$" "$project_root/packaging/fido.manifest"
grep -q "^commit=3d47260b8915385c58e20c73e24b36e9a9536f3f$" \
    "$project_root/packaging/fido.manifest"
grep -q "^linux_port_sha256=$expected_hash$" \
    "$project_root/packaging/fido.manifest"

runtime=$($project_root/run_classicsetup --print-runtime)
printf '%s\n' "$runtime" | grep -q '^pwsh=/'
printf '%s\n' "$runtime" | grep -q "^fido=$fido_script$"
pwsh_path=$(printf '%s\n' "$runtime" | sed -n 's/^pwsh=//p')

if CLASSICSETUP_PWSH=relative \
    "$project_root/run_classicsetup" --print-runtime >/dev/null 2>&1; then
    exit 1
fi
if CLASSICSETUP_FIDO_SCRIPT="$project_root/does-not-exist" \
    "$project_root/run_classicsetup" --print-runtime >/dev/null 2>&1; then
    exit 1
fi

wrong_script=$(mktemp /tmp/classicsetup-fido-wrong-XXXXXX)
trap 'rm -f "$wrong_script"' EXIT HUP INT TERM
printf 'not the pinned Fido script\n' >"$wrong_script"
if CLASSICSETUP_FIDO_SCRIPT="$wrong_script" \
    "$project_root/run_classicsetup" --print-runtime >/dev/null 2>&1; then
    exit 1
fi

mkdir -p /tmp/classicsetup-fido-test-cache \
    /tmp/classicsetup-fido-test-config \
    /tmp/classicsetup-fido-test-data
env XDG_CACHE_HOME=/tmp/classicsetup-fido-test-cache \
    XDG_CONFIG_HOME=/tmp/classicsetup-fido-test-config \
    XDG_DATA_HOME=/tmp/classicsetup-fido-test-data \
    "$pwsh_path" -NoLogo -NoProfile -NonInteractive \
    -File "$project_root/tests/fido_linux_test.ps1" \
    -ProjectRoot "$project_root"
