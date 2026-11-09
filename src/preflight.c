/* This file is part of isa-check.
 * Copyright (C) 2026 Canonical Ltd
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include <linux/errno.h>
#include <linux/reboot.h>

#include "preflight.h"

#define MAX_MISSING_FEATURES 512

#ifndef PREFLIGHT_VARIANT
#error "PREFLIGHT_VARIANT must be defined at build time"
#endif
#ifndef PREFLIGHT_FLAGS
#error "PREFLIGHT_FLAGS must be defined at build time"
#endif

static const char variant_name[] = PREFLIGHT_VARIANT;
static const char required_flags[] = PREFLIGHT_FLAGS;
static char missing_features[MAX_MISSING_FEATURES];
static char next_init[4096];

static unsigned long string_length(const char *string) {
        unsigned long length = 0;

        while (string[length])
                length++;
        return length;
}

static void write_string(const char *string) {
        unsigned long remaining = string_length(string);

        while (remaining) {
                long written = preflight_sys_write(2, string, remaining);

                if (written <= 0)
                        return;
                string += written;
                remaining -= (unsigned long) written;
        }
}

__attribute__((noreturn)) static void halt(void) {
#ifndef PREFLIGHT_TEST_NO_REBOOT
        if (preflight_sys_getpid() == 1)
                preflight_sys_reboot(LINUX_REBOOT_CMD_HALT);
#endif

        /* Stay alive if not running as init, or if reboot(2) fails. */
        for (;;)
                preflight_sys_pause();
}

__attribute__((noreturn)) void preflight_fail(const char *message) {
        write_string("\nisa-preflight: ");
        write_string(message);
        write_string("\nThe system will now halt.\n");
        halt();
}

static void prepare_proc(void) {
        long fd = preflight_sys_openat("/proc/cpuinfo");
        long result;

        if (fd >= 0) {
                preflight_sys_close((int) fd);
                return;
        }
        if (fd != -ENOENT)
                preflight_fail("could not open /proc/cpuinfo");

        result = preflight_sys_mkdir_proc();
        if (result < 0 && result != -EEXIST)
                preflight_fail("could not create /proc");
        result = preflight_sys_mount_proc();
        if (result < 0 && result != -EBUSY)
                preflight_fail("could not mount /proc");
}

__attribute__((noreturn)) void preflight_main(void *initial_stack) {
        unsigned long *stack = initial_stack;
        unsigned long argc = stack[0];
        char **argv = (char **) &stack[1];
        char **envp = argv + argc + 1;
        unsigned long i;

        prepare_proc();

        if (cpu_check(required_flags, missing_features, sizeof(missing_features))) {
                write_string(
                        "\n\n"
                        "============================================================\n"
                        "ERROR: This installation requires ");
                write_string(variant_name);
                write_string(
                        ".\n"
                        "The processor or operating system lacks required features:\n  ");
                write_string(missing_features);
                write_string(
                        "\n\n"
                        "Boot cannot continue. Use an installation that supports this\n"
                        "processor, or boot this installation on a processor that supports\n");
                write_string(variant_name);
                write_string(
                        ".\n"
                        "The system will now halt.\n"
                        "============================================================\n");
                halt();
        }

        if (preflight_read_first_line(NEXT_INIT_PATH, next_init, sizeof(next_init)) <= 0 || next_init[0] != '/')
                preflight_fail("could not read an absolute next init path from " NEXT_INIT_PATH);

        for (i = 0; next_init[i]; i++) {
                if (next_init[i] == '\r') {
                        next_init[i] = '\0';
                        break;
                }
        }

        if (preflight_sys_execve(next_init, argv, envp) < 0) {
                write_string("\nisa-preflight: the CPU check passed, but executing ");
                write_string(next_init);
                write_string(" failed.\nThe system will now halt.\n");
                halt();
        }

        /* execve only returns on failure. */
        halt();
}
