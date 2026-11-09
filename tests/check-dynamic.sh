#!/bin/sh
# Copyright (C) 2026 Canonical Ltd
# SPDX-License-Identifier: GPL-3.0-only

set -eu

binary=$1

if ! readelf -lW "$binary" | grep -q INTERP; then
    echo "$binary must have a dynamic interpreter" >&2
    exit 1
fi
if ! readelf -dW "$binary" | grep -q NEEDED; then
    echo "$binary must have dynamic dependencies" >&2
    exit 1
fi
