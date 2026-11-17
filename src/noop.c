/* This file is part of isa-check.
 * Copyright (C) 2026 Canonical Ltd
 * SPDX-License-Identifier: GPL-3.0-only
 */
/* Non-variant initramfs entry point: chain-load the original init without
 * inspecting CPU features. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sys/reboot.h>
#include <unistd.h>

#include "preflight.h"

__attribute__((noreturn)) static void halt(void) {
#ifdef PREFLIGHT_TEST_EXIT_ON_HALT
        exit(1);
#else
        if (getpid() == 1)
                reboot(RB_HALT_SYSTEM);
        for (;;)
                pause();
#endif
}

int main(int argc, char **argv, char **envp) {
        (void) argc;
        execve(NEXT_INIT_PATH, argv, envp);
        fputs("\nisa-preflight: executing " NEXT_INIT_PATH " failed\nThe system will now halt.\n", stderr);
        halt();
}
