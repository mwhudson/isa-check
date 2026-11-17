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

# Resolve each component inside the image, including absolute symlinks in
# parent directories. Host realpath/test must never follow an image symlink.
preinit_resolve_init() {
    local _root=$1 _pending=init _resolved='' _component _link
    local _links=0

    while [[ -n $_pending ]]; do
        _component=${_pending%%/*}
        if [[ $_pending == */* ]]; then
            _pending=${_pending#*/}
        else
            _pending=
        fi
        case $_component in
            ''|.) continue ;;
            ..) _resolved=${_resolved%/*}; continue ;;
        esac
        if [[ -L $_root$_resolved/$_component ]]; then
            ((_links += 1))
            [[ $_links -le 40 ]] || return 1
            _link=$(readlink -- "$_root$_resolved/$_component") || return 1
            [[ $_link != /* ]] || _resolved=
            # Preserve a trailing slash's requirement that the target be a directory.
            [[ $_link != */ ]] || _link+=.
            _pending=${_link#/}${_pending:+/$_pending}
        else
            [[ -e $_root$_resolved/$_component ]] || return 1
            if [[ -n $_pending ]]; then
                [[ -d $_root$_resolved/$_component ]] || return 1
            fi
            _resolved+=/$_component
        fi
    done
    [[ -f $_root$_resolved && -x $_root$_resolved ]] || return 1
    printf '%s\n' "$_resolved"
}

install() {
    local _checker=${DRACUT_PREINIT:-${dracutsysrootdir-}/usr/lib/isa-preflight/isa-preflight}
    local _next_init
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

    if [[ -L $_root/init ]]; then
        if ! _next_init=$(preinit_resolve_init "$_root"); then
            dfatal "The preinit module requires an executable init target"
            exit 1
        fi
    elif [[ -f $_root/init && -x $_root/init ]]; then
        if [[ -e $_root/init.dracut || -L $_root/init.dracut ]]; then
            dfatal "Cannot preserve the original init: /init.dracut already exists"
            exit 1
        fi
        _next_init=/init.dracut
        mv "$_root/init" "$_root/init.dracut" || exit 1
    else
        dfatal "The preinit module requires an existing executable init"
        exit 1
    fi

    mkdir -p "$_root/etc/isa-preflight" || exit 1
    printf '%s\n' "$_next_init" > "$_root/etc/isa-preflight/next-init" || exit 1

    # Do not let installation follow an existing init symlink and overwrite
    # its target. Resolve dependencies immediately into this root, since the
    # outer squash loader cannot use libraries from the inner image. Variant
    # checkers are freestanding; the no-op uses the image's dynamic runtime.
    rm -f "$_root/init" || exit 1
    DRACUT_RESOLVE_LAZY="" dstdir="$_root" inst "$_checker" /init || exit 1
}
