#!/bin/bash
# Copyright (C) 2026 Canonical Ltd
# SPDX-License-Identifier: GPL-3.0-only

set -eu

checker=$(readlink -f "$1")
module=$2
test_root=$(mktemp -d)
trap 'rm -rf "$test_root"' EXIT

dracutsysrootdir=
DRACUT_PREINIT=$checker
squash_active=no
DRACUT_RESOLVE_LAZY=yes

dracut_module_included() {
    [[ $1 == squash-lib && $squash_active == yes ]]
}

dfatal() {
    printf '%s\n' "$*" >&2
}

inst_simple() {
    [[ $# -eq 2 ]]
    [[ -n ${dstdir-} ]]
    if [[ $1 == /* && -n ${dracutsysrootdir-} ]]; then
        [[ -e ${dracutsysrootdir}/${1#"${dracutsysrootdir}"} ]] || return 1
    fi
    command install -D -m 0755 "$1" "$dstdir$2"
}

inst() {
    # The production module must resolve dependencies into dstdir immediately.
    [[ -z $DRACUT_RESOLVE_LAZY ]]
    inst_simple "$@"
}

# shellcheck disable=SC1090
source "$module"

setup_case() {
    rm -rf "$test_root/root" "$test_root/squash"
    mkdir -p "$test_root/root" "$test_root/squash"
    initdir="$test_root/root"
    squashdir="$test_root/squash"
    squash_active=no
}

make_systemd_init() {
    mkdir -p "$1/usr/lib/systemd"
    printf 'systemd\n' > "$1/usr/lib/systemd/systemd"
    chmod 0755 "$1/usr/lib/systemd/systemd"
    ln -s usr/lib/systemd/systemd "$1/init"
}

make_base_init() {
    printf '#!/bin/sh\necho original init\n' > "$1/init"
    chmod 0755 "$1/init"
}

check
DRACUT_PREINIT=relative-path
if check; then
    echo 'A relative DRACUT_PREINIT was accepted' >&2
    exit 1
fi
DRACUT_PREINIT=$checker

# A systemd symlink is moved unchanged to /next-init and preflight becomes /init.
setup_case
make_systemd_init "$initdir"
install
[[ ! -L $initdir/init ]]
cmp "$checker" "$initdir/init"
[[ -L $initdir/next-init && $(readlink "$initdir/next-init") == usr/lib/systemd/systemd ]]

# A regular init script is moved unchanged to /next-init.
setup_case
make_base_init "$initdir"
install
cmp "$checker" "$initdir/init"
[[ $(< "$initdir/next-init") == $'#!/bin/sh\necho original init' ]]

# A relative, non-canonical sysroot is valid; only an override must be absolute.
setup_case
mkdir -p "$test_root/sysroot/usr/lib/isa-preflight"
command install -m 0755 "$checker" "$test_root/sysroot/usr/lib/isa-preflight/isa-preflight"
ln -s sysroot "$test_root/sysroot-link"
dracutsysrootdir=$(realpath -s --relative-to="$PWD" "$test_root/sysroot-link")
DRACUT_PREINIT=
check
make_base_init "$initdir"
install
cmp "$checker" "$initdir/init"
DRACUT_PREINIT=$checker
dracutsysrootdir=

# With squash-lib, wrap only the outer loader and leave the inner init alone.
setup_case
make_systemd_init "$initdir"
make_base_init "$squashdir"
squash_active=yes
install
[[ $(readlink "$initdir/init") == usr/lib/systemd/systemd ]]
[[ ! -e $initdir/next-init && ! -L $initdir/next-init ]]
cmp "$checker" "$squashdir/init"
[[ -f $squashdir/next-init && -x $squashdir/next-init ]]

# Never overwrite an existing /next-init, and require an init to wrap.
setup_case
make_base_init "$initdir"
: > "$initdir/next-init"
if (install) 2> /dev/null; then
    echo 'A /next-init collision was accepted' >&2
    exit 1
fi

setup_case
if (install) 2> /dev/null; then
    echo 'A missing /init was accepted' >&2
    exit 1
fi
