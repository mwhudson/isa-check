/* This file is part of isa-check.
 * Copyright (C) 2026 Canonical Ltd
 * SPDX-License-Identifier: GPL-3.0-only
 */
/* Adapt libc file operations to the negative-errno I/O interface used by the
 * shared CPU parser in the normal userspace isa-check executable. */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "cpuinfo.h"

long isa_sys_read(int fd, void *buffer, unsigned long count) {
        long result = read(fd, buffer, count);

        return result < 0 ? -errno : result;
}

long isa_sys_open_readonly(const char *path) {
        long result = open(path, O_RDONLY);

        return result < 0 ? -errno : result;
}

long isa_sys_close(int fd) {
        long result = close(fd);

        return result < 0 ? -errno : result;
}
