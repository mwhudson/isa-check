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

# A systemd symlink is replaced by preflight, which executes its real target.
setup_case
make_systemd_init "$initdir"
install
[[ ! -L $initdir/init ]]
cmp "$checker" "$initdir/init"
[[ $(< "$initdir/etc/isa-preflight/next-init") == /usr/lib/systemd/systemd ]]
[[ ! -e $initdir/init.dracut && ! -L $initdir/init.dracut ]]

# Absolute initramfs links must not be resolved against the build host.
setup_case
make_systemd_init "$initdir"
rm "$initdir/init"
ln -s /usr/lib/systemd/systemd "$initdir/init"
install
cmp "$checker" "$initdir/init"
[[ $(< "$initdir/etc/isa-preflight/next-init") == /usr/lib/systemd/systemd ]]

# Follow nested absolute links inside the image, even when absent on the host.
setup_case
mkdir -p "$initdir/sbin" "$initdir/usr/lib/isa-preflight-test"
printf '#!/bin/sh\nexit 0\n' > "$initdir/usr/lib/isa-preflight-test/init"
chmod 0755 "$initdir/usr/lib/isa-preflight-test/init"
ln -s sbin/init "$initdir/init"
ln -s /usr/lib/isa-preflight-test/init "$initdir/sbin/init"
install
[[ $(< "$initdir/etc/isa-preflight/next-init") == /usr/lib/isa-preflight-test/init ]]

# Resolve symlinked parent directories and relative targets before interpreting '..'.
setup_case
mkdir -p "$initdir/usr/lib/isa-preflight-test" "$initdir/usr/sbin"
printf '#!/bin/sh\nexit 0\n' > "$initdir/usr/lib/isa-preflight-test/init"
chmod 0755 "$initdir/usr/lib/isa-preflight-test/init"
ln -s /usr/sbin "$initdir/sbin"
ln -s ../lib/isa-preflight-test/init "$initdir/usr/sbin/init"
ln -s /sbin/init "$initdir/init"
install
[[ $(< "$initdir/etc/isa-preflight/next-init") == /usr/lib/isa-preflight-test/init ]]

# A host executable cannot make an image's broken absolute link valid.
setup_case
mkdir "$initdir/sbin"
ln -s /bin/sh "$initdir/sbin/init"
ln -s sbin/init "$initdir/init"
if (install) 2> /dev/null; then
    echo 'A host-only init target was accepted' >&2
    exit 1
fi

# Reject loops and a trailing slash on an executable file.
setup_case
ln -s /init "$initdir/init"
if (install) 2> /dev/null; then
    echo 'An init symlink loop was accepted' >&2
    exit 1
fi
setup_case
make_systemd_init "$initdir"
rm "$initdir/init"
ln -s /usr/lib/systemd/systemd/ "$initdir/init"
if (install) 2> /dev/null; then
    echo 'A non-directory init target with a trailing slash was accepted' >&2
    exit 1
fi

# A regular base init script is preserved under its recovery-friendly name.
setup_case
make_base_init "$initdir"
install
cmp "$checker" "$initdir/init"
[[ -f $initdir/init.dracut && -x $initdir/init.dracut ]]
[[ $(< "$initdir/etc/isa-preflight/next-init") == /init.dracut ]]

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
[[ -L $initdir/init ]]
[[ $(readlink "$initdir/init") == usr/lib/systemd/systemd ]]
[[ ! -e $initdir/etc/isa-preflight/next-init ]]
cmp "$checker" "$squashdir/init"
[[ -f $squashdir/init.dracut && -x $squashdir/init.dracut ]]
[[ $(< "$squashdir/etc/isa-preflight/next-init") == /init.dracut ]]

# Never overwrite a pre-existing recovery init or follow a broken init link.
setup_case
make_base_init "$initdir"
: > "$initdir/init.dracut"
if (install) 2> /dev/null; then
    echo 'An init.dracut collision was accepted' >&2
    exit 1
fi

setup_case
ln -s usr/lib/systemd/missing "$initdir/init"
if (install) 2> /dev/null; then
    echo 'A broken init symlink was accepted' >&2
    exit 1
fi
