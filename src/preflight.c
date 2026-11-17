/* This file is part of isa-check.
 * Copyright (C) 2026 Canonical Ltd
 * SPDX-License-Identifier: GPL-3.0-only
 */
/* Freestanding variant initramfs entry point: check CPU features before any
 * ISA-dependent userspace executes, then hand off or diagnose and halt. */
#include <linux/errno.h>
#include <linux/reboot.h>

#include "cpuinfo.h"
#include "preflight.h"

#ifndef PREFLIGHT_VARIANT
#error "PREFLIGHT_VARIANT must be defined at build time"
#endif
#ifndef PREFLIGHT_FLAGS
#error "PREFLIGHT_FLAGS must be defined at build time"
#endif

static const char variant_name[] = PREFLIGHT_VARIANT;
static const char required_flags[] = PREFLIGHT_FLAGS;
static char cpu_flags[CPU_FLAGS_SIZE];
static char missing_features[MISSING_FEATURES_SIZE];

static void write_string(const char *string) {
        unsigned long remaining = isa_string_length(string);

        while (remaining) {
                long written = preflight_sys_write(2, string, remaining);

                if (written <= 0)
                        return;
                string += written;
                remaining -= (unsigned long) written;
        }
}

__attribute__((noreturn)) static void halt(void) {
#ifdef PREFLIGHT_TEST_EXIT_ON_HALT
        preflight_sys_exit(1);
#else
        if (preflight_sys_getpid() == 1)
                preflight_sys_reboot(LINUX_REBOOT_CMD_HALT);

        /* Stay alive if not running as init, or if reboot(2) fails. */
        for (;;)
                preflight_sys_pause();
#endif
}

__attribute__((noreturn)) static void fail(const char *message) {
        write_string("\nisa-preflight: ");
        write_string(message);
        write_string("\nThe system will now halt.\n");
        halt();
}

/* Mount proc if it is not already available. Returns 1 if it was mounted here. */
static int prepare_proc(void) {
        long fd = isa_sys_open_readonly(PROC_DIR "/cpuinfo");
        long result;

        if (fd >= 0) {
                isa_sys_close((int) fd);
                return 0;
        }
        if (fd != -ENOENT)
                fail("could not open " PROC_DIR "/cpuinfo");

        result = preflight_sys_mkdir(PROC_DIR, 0755);
        if (result < 0 && result != -EEXIST)
                fail("could not create " PROC_DIR);
        result = preflight_sys_mount_proc(PROC_DIR);
        if (result == -EBUSY)
                return 0;
        if (result < 0)
                fail("could not mount " PROC_DIR);
        return 1;
}

__attribute__((noreturn)) void preflight_main(void *initial_stack) {
        unsigned long *stack = initial_stack;
        unsigned long argc = stack[0];
        char **argv = (char **) &stack[1];
        char **envp = argv + argc + 1;
        int mounted_proc;
        int result;

        mounted_proc = prepare_proc();

        result = cpu_read_flags(cpu_flags, sizeof(cpu_flags));
        if (result < 0)
                fail(cpu_error_message(result));
        result = cpu_missing_features(cpu_flags, required_flags, missing_features, sizeof(missing_features));
        if (result < 0)
                fail(cpu_error_message(result));
        if (result > 0) {
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

        /* Hand off the same environment as the no-op build. Failing to unmount
         * leaves an extra proc mount, which is not worth halting the boot for. */
        if (mounted_proc)
                preflight_sys_umount(PROC_DIR);

        preflight_sys_execve(NEXT_INIT_PATH, argv, envp);

        /* execve only returns on failure. */
        fail("the CPU check passed, but executing " NEXT_INIT_PATH " failed");
}
