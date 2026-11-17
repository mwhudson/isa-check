/* This file is part of isa-check.
 * Copyright (C) 2026 Canonical Ltd
 * SPDX-License-Identifier: GPL-3.0-only
 */
/* Non-variant initramfs entry point: use libc to read the successor
 * path and chain-load it without inspecting CPU features. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/reboot.h>
#include <unistd.h>

#include "preflight.h"

static char next_init[NEXT_INIT_SIZE];

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

__attribute__((noreturn)) static void fail(const char *message) {
        fprintf(stderr, "\nisa-preflight: %s\nThe system will now halt.\n", message);
        halt();
}

int main(int argc, char **argv, char **envp) {
        FILE *file;

        (void) argc;
        file = fopen(NEXT_INIT_PATH, "r");
        if (!file)
                fail("could not open " NEXT_INIT_PATH);
        if (!fgets(next_init, sizeof(next_init), file)) {
                fclose(file);
                fail("could not read " NEXT_INIT_PATH);
        }
        /* A line which exactly fills the buffer still fits if it ends here. */
        if (!strchr(next_init, '\n')) {
                int c = getc(file);

                if (c != EOF && c != '\n') {
                        fclose(file);
                        fail("next init path is too long");
                }
        }
        fclose(file);

        next_init[strcspn(next_init, "\r\n")] = '\0';
        if (next_init[0] != '/')
                fail("could not read an absolute next init path from " NEXT_INIT_PATH);

        execve(next_init, argv, envp);
        fprintf(stderr, "\nisa-preflight: executing %s failed.\nThe system will now halt.\n", next_init);
        halt();
}
