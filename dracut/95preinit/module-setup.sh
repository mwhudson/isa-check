#!/bin/bash
# This file is part of isa-check.
# Copyright (C) 2026 Canonical Ltd
# SPDX-License-Identifier: GPL-3.0-only

# Run after the modules which install /init. In particular, this must follow
# 10systemd, 80base, and the initial installation pass of 88squash-lib. Leave
# 96-99 free for modules which need to run after this one.
check() {
    local _checker=${DRACUT_PREINIT:-${dracutsysrootdir-}/usr/lib/isa-preflight/isa-preflight}

    if [[ ${DRACUT_PREINIT-} && $_checker != /* ]]; then
        return 1
    fi
    [[ -x $_checker ]]
}

install() {
    local _checker=${DRACUT_PREINIT:-${dracutsysrootdir-}/usr/lib/isa-preflight/isa-preflight}
    local _root=$initdir

    if dracut_module_included "squash-lib"; then
        # The outer squash loader is the kernel's first init. Leave the inner
        # init untouched and put preflight before the outer loader instead.
        _root=$squashdir
    fi
    _root=$(readlink -f -- "$_root") || exit 1

    if [[ ${DRACUT_PREINIT-} && $_checker != /* ]] || [[ ! -f $_checker || ! -x $_checker ]]; then
        dfatal "The preinit module could not find its architecture checker"
        exit 1
    fi
    if [[ ! -e $_root/init && ! -L $_root/init ]]; then
        dfatal "The preinit module requires an existing /init"
        exit 1
    fi
    if [[ -e $_root/next-init || -L $_root/next-init ]]; then
        dfatal "Cannot preserve the original init: /next-init already exists"
        exit 1
    fi

    # isa-preflight executes /next-init. Moving /init keeps a symlinked init
    # pointing at the same target, since both names are in the image root.
    mv "$_root/init" "$_root/next-init" || exit 1

    # Resolve dependencies immediately into this root, since the outer squash
    # loader cannot use libraries from the inner image. Variant checkers are
    # freestanding; the no-op uses the image's dynamic runtime.
    DRACUT_RESOLVE_LAZY="" dstdir="$_root" inst "$_checker" /init || exit 1
}
