#!/bin/bash
# Copyright (C) 2026 Canonical Ltd
# SPDX-License-Identifier: GPL-3.0-only

set -eu

binary=$1
test_root=$2
table=$3

rm -rf "$test_root"
mkdir -p "$test_root/bin"

# Simulate native architectures independently of the build/test host.
cat > "$test_root/bin/dpkg" << 'EOF'
#!/bin/sh
[ "$#" -eq 1 ] && [ "$1" = --print-architecture ] || exit 1
[ "${ISA_TEST_DPKG_FAILURE:-0}" = 0 ] || exit 1
printf '%s\n' "${ISA_TEST_ARCH-amd64}"
EOF
chmod 0755 "$test_root/bin/dpkg"
export PATH="$test_root/bin:$PATH"

v3_flags=$(awk '$1 == "amd64v3" { print $3 }' "$table")
v2_flags=$(awk '$1 == "amd64v2" { print $3 }' "$table")
v3_cpu=${v3_flags//,/ }
v2_cpu=${v2_flags//,/ }

# A v3-only system lists every compatible variant in table order.
printf 'processor\t: 0\nflags\t\t: %s\n' "$v3_cpu" > "$test_root/cpuinfo"
output=$($binary --table "$table")
[[ $output == $'amd64v3\namd64v2\namd64v1' ]]
output=$($binary --table "$table" amd64v3)
[[ -z $output ]]

set +e
output=$($binary --table "$table" amd64v4 2>&1)
status=$?
set -e
[[ $status -eq 1 ]]
[[ $output == *'amd64v4 is not supported:'*avx512f* ]]

# A system is only compatible with variants supported by every processor.
{
    printf 'processor\t: 0\nflags\t\t: %s\n' "$v3_cpu"
    printf 'processor\t: 1\nflags\t\t: %s\n' "$v2_cpu"
} > "$test_root/cpuinfo"
output=$($binary --table "$table")
[[ $output == $'amd64v2\namd64v1' ]]
set +e
output=$($binary --table "$table" amd64v3 2>&1)
status=$?
set -e
[[ $status -eq 1 ]]
[[ $output == *avx2* ]]

# No matching variants and table/usage errors have distinct statuses.
printf 'processor\t: 0\nflags\t\t:\n' > "$test_root/cpuinfo"
set +e
output=$($binary --table "$table")
status=$?
set -e
[[ $status -eq 1 ]]
[[ -z $output ]]
set +e
output=$($binary --table "$test_root/missing" 2>&1)
status=$?
set -e
[[ $status -eq 2 ]]
[[ $output == *'could not open the variant table'* ]]
set +e
output=$($binary --table "$table" not-a-variant 2>&1)
status=$?
set -e
[[ $status -eq 2 ]]
[[ $output == *'variant is not present in the table'* ]]
$binary --help | grep -q '^Usage: isa-check'
$binary -h | grep -q '^Usage: isa-check'
set +e
output=$($binary --bogus 2>&1)
status=$?
set -e
[[ $status -eq 2 ]]
[[ $output == 'Usage: isa-check'* ]]

# Option parsing stops at --.
printf 'processor\t: 0\nflags\t\t: %s\n' "$v3_cpu" > "$test_root/cpuinfo"
$binary --table "$table" -- amd64v3

# Output errors are not reported as success.
set +e
output=$($binary --table "$table" 2>&1 > /dev/full)
status=$?
set -e
[[ $status -eq 2 ]]
[[ $output == *'could not write output'* ]]

# Long lines other than flags lines are irrelevant, but long flags lines are errors.
long=$(printf '%10000s' '' | tr ' ' x)
printf 'processor\t: 0\nbugs\t\t: %s\nflags\t\t: %s\n' "$long" "$v3_cpu" > "$test_root/cpuinfo"
output=$($binary --table "$table")
[[ $output == $'amd64v3\namd64v2\namd64v1' ]]
printf 'processor\t: 0\nflags\t\t: %s %s\n' "$v3_cpu" "$long" > "$test_root/cpuinfo"
set +e
output=$($binary --table "$table" 2>&1)
status=$?
set -e
[[ $status -eq 2 ]]
[[ $output == *'are too long'* ]]

# Missing features are listed in full, comma-separated.
printf 'processor\t: 0\nflags\t\t: %s\n' "$v2_cpu" > "$test_root/cpuinfo"
set +e
output=$($binary --table "$table" amd64v3 2>&1)
set -e
[[ $output == 'amd64v3 is not supported: avx, avx2, bmi1, bmi2, f16c, fma, abm, movbe, xsave' ]]

# Missing cpuinfo is a read error, not a reason for the probe to mount /proc.
rm -f "$test_root/cpuinfo"
set +e
output=$($binary --table "$table" amd64v3 2>&1)
status=$?
set -e
[[ $status -eq 2 ]]
[[ $output == *"could not open $test_root/cpuinfo"* ]]

# No native variants is an empty result, without needing CPU feature fields.
status=0
output=$(ISA_TEST_ARCH=arm64 $binary --table "$table") || status=$?
[[ $status -eq 1 && -z $output ]]

# New arm64 table rows work without rebuilding the CLI. Features from every
# processor must be present; both colon-adjacent and spaced keys are accepted.
cat > "$test_root/arm64-table" << 'EOF'
arm64-test-sve arm64 fp,asimd,sve
arm64-test-base arm64 fp,asimd
amd64-test amd64 lm
EOF
printf 'processor\t: 0\nFeatures: fp asimd sve\nprocessor\t: 1\nFeatures   : fp asimd sve\n' > "$test_root/cpuinfo"
output=$(ISA_TEST_ARCH=arm64 $binary --table "$test_root/arm64-table")
[[ $output == $'arm64-test-sve\narm64-test-base' ]]
ISA_TEST_ARCH=arm64 $binary --table "$test_root/arm64-table" arm64-test-sve
printf 'processor\t: 0\nFeatures\t: fp asimd sve\nprocessor\t: 1\nFeatures\t: fp asimd\n' > "$test_root/cpuinfo"
output=$(ISA_TEST_ARCH=arm64 $binary --table "$test_root/arm64-table")
[[ $output == arm64-test-base ]]
status=0
output=$(ISA_TEST_ARCH=arm64 $binary --table "$test_root/arm64-table" arm64-test-sve 2>&1) || status=$?
[[ $status -eq 1 && $output == 'arm64-test-sve is not supported: sve' ]]

# Bad architecture discovery is an input error, not a compatibility result.
status=0
output=$(ISA_TEST_DPKG_FAILURE=1 $binary --table "$table" 2>&1) || status=$?
[[ $status -eq 2 && $output == *'could not determine the native architecture'* ]]
status=0
output=$(ISA_TEST_ARCH='invalid architecture' $binary --table "$table" 2>&1) || status=$?
[[ $status -eq 2 && $output == *'invalid native architecture'* ]]

rm -rf "$test_root"
