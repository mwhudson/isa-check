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
    # Make proc look mounted; the mount path is exercised separately below.
    mkdir -p "$test_root/proc"
    : > "$test_root/proc/cpuinfo"
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
    output=$("$binary" must-not-run 2>&1)
    status=$?
    set -e
    [[ $status -eq 1 ]]
    [[ $output == *'requires amd64v3'* ]]
    [[ $output == *avx2* ]]
    [[ $output != *NEXT_INIT_RAN* ]]

    # Long lines other than flags lines are irrelevant to the check.
    long=$(printf '%5000s' '' | tr ' ' x)
    printf 'processor\t: 0\nbugs\t\t: %s\nflags\t\t: %s\n' "$long" "$cpu_flags" > "$test_root/cpuinfo"
    output=$(PREFLIGHT_MARKER=unit-test "$binary" first second)
    [[ $output == 'NEXT_INIT_RAN argv=first second marker=unit-test' ]]
    printf 'processor\t: 0\nflags\t\t: %s\n' "$cpu_flags" > "$test_root/cpuinfo"
fi

# Both builds accept a successor path that exactly fills the 4096-byte buffer,
# with or without a final newline, and reject one byte more.
long_dir=$test_root
while (( 4095 - ${#long_dir} - 1 > 255 )); do
    long_dir+=/$(printf '%200s' '' | tr ' ' d)
done
mkdir -p "$long_dir"
long_init=$long_dir/$(printf "%$((4095 - ${#long_dir} - 1))s" '' | tr ' ' i)
[[ ${#long_init} -eq 4095 ]]
cp "$test_root/next-init" "$long_init"
for terminator in '\n' ''; do
    printf "%s$terminator" "$long_init" > "$config/next-init"
    output=$(PREFLIGHT_MARKER=unit-test "$binary" first second)
    [[ $output == 'NEXT_INIT_RAN argv=first second marker=unit-test' ]]
done
printf '%sx\n' "$long_init" > "$config/next-init"
set +e
output=$("$binary" 2>&1)
status=$?
set -e
[[ $status -eq 1 ]]
[[ $output != *NEXT_INIT_RAN* ]]
printf '%s\n' "$test_root/next-init" > "$config/next-init"

# The successor is deliberately runtime configuration, not a compiled path.
printf '%s\n' relative-init > "$config/next-init"
set +e
output=$("$binary" 2>&1)
status=$?
set -e
[[ $status -eq 1 ]]
[[ $output == *'could not read an absolute next init path'* ]]
printf '%s\n' "$test_root/next-init" > "$config/next-init"

# Without proc, the variant build mounts it and unmounts it before handing off.
# This needs unprivileged user and mount namespaces, which may be unavailable.
if [[ $base != yes ]]; then
    rm -rf "$test_root/proc"
    if unshare -rmpf sh -c 'mkdir "$1" && mount -t proc proc "$1"' sh "$test_root/proc" 2> /dev/null; then
        rmdir "$test_root/proc"
        cat > "$test_root/next-init" << EOF
#!/bin/sh
if [ -e '$test_root/proc/self' ]; then echo proc-mounted; else echo proc-unmounted; fi
EOF
        output=$(unshare -rmpf "$binary")
        [[ $output == proc-unmounted ]]
        [[ -d $test_root/proc ]]
    else
        echo 'Skipping the proc mount test: namespaces are unavailable.' >&2
    fi
fi

rm -rf "$test_root"
