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
    # Moving a symlinked init keeps it valid, since both names are in the root.
    mv "$_root/init" "$_root/init.test-real"
    _real_init=/init.test-real
    dstdir="$_root" inst_simple "$ISA_TEST_OBSERVER" /init
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
