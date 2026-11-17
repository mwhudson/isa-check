#!/bin/bash
# Copyright (C) 2026 Canonical Ltd
# SPDX-License-Identifier: GPL-3.0-only

# This test-only module runs between the init provider and the installed preinit
# module. Its observer checks handoff before executing the real init provider.
check() { return 255; }

install() {
    local _root=$initdir _real_init

    if dracut_module_included squash-lib; then
        _root=$squashdir
    fi
    if [[ -L $_root/init ]]; then
        # Use the installed resolver to keep all links inside this image.
        # Source it in a subshell so that its check() and install() do not
        # replace this module's functions.
        # shellcheck disable=SC1091
        _real_init=$(
            source /usr/lib/dracut/modules.d/99preinit/module-setup.sh
            preinit_resolve_init "$_root"
        ) || exit 1
        rm "$_root/init"
        ln -s /usr/lib/isa-preflight-test/observer "$_root/init"
        dstdir="$_root" inst_simple "$ISA_TEST_OBSERVER" /usr/lib/isa-preflight-test/observer
    else
        mv "$_root/init" "$_root/init.test-real"
        _real_init=/init.test-real
        dstdir="$_root" inst_simple "$ISA_TEST_OBSERVER" /init
    fi
    mkdir -p "$_root/etc/isa-preflight-test"
    printf '%s\n' "$_real_init" > "$_root/etc/isa-preflight-test/real-init"

    # The inner provider reports that it ran and powers off the emulated guest.
    inst_simple /bin/busybox /usr/lib/isa-preflight-test/busybox
    inst_simple "$ISA_TEST_FINISH" /usr/lib/isa-preflight-test/finish
    if [[ $ISA_TEST_PROVIDER == shell ]]; then
        inst_hook cmdline 00 "$ISA_TEST_FINISH"
    else
        inst_simple "$ISA_TEST_SERVICE" /etc/systemd/system/isa-preflight-test.service
        mkdir -p "$initdir/etc/systemd/system/initrd.target.wants"
        ln -s ../isa-preflight-test.service \
            "$initdir/etc/systemd/system/initrd.target.wants/isa-preflight-test.service"
    fi
}
