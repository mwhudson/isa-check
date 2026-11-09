#!/bin/bash
# Copyright (C) 2026 Canonical Ltd
# SPDX-License-Identifier: GPL-3.0-only

set -eu

binary=$1
test_root=$2
flags=$3
base=${4:-no}
config="$test_root/config"

rm -rf "$test_root"
mkdir -p "$config"

cat > "$test_root/next-init" << 'EOF'
#!/bin/sh
echo "NEXT_INIT_RAN argv=$* marker=$PREFLIGHT_MARKER"
EOF
chmod 0755 "$test_root/next-init"
printf '%s\n' "$test_root/next-init" > "$config/next-init"

cpu_flags=${flags//,/ }
if [[ $base == yes ]]; then
    # A base build must not consult cpuinfo at all.
    rm -f "$test_root/cpuinfo"
else
    printf 'processor\t: 0\nflags\t\t: %s\n' "$cpu_flags" > "$test_root/cpuinfo"
fi
output=$(PREFLIGHT_MARKER=unit-test "$binary" first second)
[[ $output == 'NEXT_INIT_RAN argv=first second marker=unit-test' ]]

# Both builds must accept the same successor path with CRLF line endings.
printf '%s\r\n' "$test_root/next-init" > "$config/next-init"
output=$(PREFLIGHT_MARKER=unit-test "$binary" first second)
[[ $output == 'NEXT_INIT_RAN argv=first second marker=unit-test' ]]
printf '%s\n' "$test_root/next-init" > "$config/next-init"

if [[ $base != yes ]]; then
    # Every processor must satisfy the complete requirement set.
    unsupported=${cpu_flags/avx2 /}
    {
        printf 'processor\t: 0\nflags\t\t: %s\n' "$cpu_flags"
        printf 'processor\t: 1\nflags\t\t: %s\n' "$unsupported"
    } > "$test_root/cpuinfo"
    set +e
    output=$(timeout 2 "$binary" must-not-run 2>&1)
    status=$?
    set -e
    [[ $status -eq 124 ]]
    [[ $output == *'requires amd64v3'* ]]
    [[ $output == *avx2* ]]
    [[ $output != *NEXT_INIT_RAN* ]]
fi

# The successor is deliberately runtime configuration, not a compiled path.
if [[ $base != yes ]]; then
    printf 'processor\t: 0\nflags\t\t: %s\n' "$cpu_flags" > "$test_root/cpuinfo"
fi
printf '%s\n' relative-init > "$config/next-init"
set +e
output=$(timeout 2 "$binary" 2>&1)
status=$?
set -e
[[ $status -eq 124 ]]
[[ $output == *'could not read an absolute next init path'* ]]

rm -rf "$test_root"
