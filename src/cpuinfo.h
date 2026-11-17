/* This file is part of isa-check.
 * Copyright (C) 2026 Canonical Ltd
 * SPDX-License-Identifier: GPL-3.0-only
 */
#ifndef ISA_CPUINFO_H
#define ISA_CPUINFO_H

#ifndef CPUINFO_PATH
#define CPUINFO_PATH "/proc/cpuinfo"
#endif

/* Capacity for the flags common to every processor, and for one cpuinfo line. */
#define CPU_FLAGS_SIZE 8192
/* Capacity for a comma-separated list of missing features. */
#define MISSING_FEATURES_SIZE 512

/* I/O backend, supplied by libc.c or the freestanding x86-64 backend.
 * Each returns a negative errno value on failure. */
long isa_sys_read(int fd, void *buffer, unsigned long count);
long isa_sys_open_readonly(const char *path);
long isa_sys_close(int fd);

enum cpu_error {
        CPU_ERROR_OPEN = -1,
        CPU_ERROR_READ = -2,
        CPU_ERROR_NO_FLAGS = -3,
        CPU_ERROR_MALFORMED = -4,
        CPU_ERROR_TOO_LONG = -5,
        CPU_ERROR_INVALID_REQUIREMENTS = -6,
};

/* Describe a negative cpu_error value. */
const char *cpu_error_message(int error);

/* Store features common to all "flags" or "Features" fields in CPUINFO_PATH.
 * Returns 0 or a negative cpu_error value. */
int cpu_read_flags(char *flags, unsigned long capacity);

/* Check a comma-separated requirement list against flags from cpu_read_flags.
 * Returns the number of missing features, listed in missing, or a negative
 * cpu_error value. */
int cpu_missing_features(const char *flags, const char *required, char *missing, unsigned long missing_capacity);

unsigned long isa_string_length(const char *string);

#endif
