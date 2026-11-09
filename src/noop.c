/* This file is part of isa-check.
 * Copyright (C) 2026 Canonical Ltd
 * SPDX-License-Identifier: GPL-3.0-only
 */
#define _GNU_SOURCE
#include <linux/reboot.h>
#include <stdio.h>
#include <string.h>
#include <sys/reboot.h>
#include <unistd.h>

#ifndef CONFIG_DIR
#define CONFIG_DIR "/etc/isa-preflight"
#endif
#define NEXT_INIT_PATH CONFIG_DIR "/next-init"

static char next_init[4096];

__attribute__((noreturn)) static void halt(void) {
#ifndef PREFLIGHT_TEST_NO_REBOOT
        if (getpid() == 1)
                reboot(LINUX_REBOOT_CMD_HALT);
#endif
        for (;;)
                pause();
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
        if (!strchr(next_init, '\n') && !feof(file)) {
                fclose(file);
                fail("next init path is too long");
        }
        fclose(file);

        next_init[strcspn(next_init, "\r\n")] = '\0';
        if (next_init[0] != '/')
                fail("could not read an absolute next init path from " NEXT_INIT_PATH);

        execve(next_init, argv, envp);
        fprintf(stderr, "\nisa-preflight: executing %s failed.\nThe system will now halt.\n", next_init);
        halt();
}
