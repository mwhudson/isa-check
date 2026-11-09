/* This file is part of isa-check.
 * Copyright (C) 2026 Canonical Ltd
 * SPDX-License-Identifier: GPL-3.0-only
 */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "preflight.h"

long preflight_sys_read(int fd, void *buffer, unsigned long count) {
        long result = read(fd, buffer, count);

        return result < 0 ? -errno : result;
}

long preflight_sys_openat(const char *path) {
        long result = open(path, O_RDONLY);

        return result < 0 ? -errno : result;
}

long preflight_sys_close(int fd) {
        long result = close(fd);

        return result < 0 ? -errno : result;
}
