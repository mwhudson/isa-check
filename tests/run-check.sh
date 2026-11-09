#!/bin/bash
# Copyright (C) 2026 Canonical Ltd
# SPDX-License-Identifier: GPL-3.0-only

set -eu

binary=$1
test_root=$2
table=$3

rm -rf "$test_root"
mkdir -p "$test_root"

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

# Missing cpuinfo is a read error, not a reason for the probe to mount /proc.
rm -f "$test_root/cpuinfo"
set +e
output=$($binary --table "$table" amd64v3 2>&1)
status=$?
set -e
[[ $status -eq 2 ]]
[[ $output == *"could not open $test_root/cpuinfo"* ]]

rm -rf "$test_root"
