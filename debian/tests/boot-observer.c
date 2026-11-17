/* Copyright (C) 2026 Canonical Ltd
 * SPDX-License-Identifier: GPL-3.0-only
 */
/* QEMU guest successor: verify the production checker's PID 1 handoff and proc
 * cleanup, then execute the original dracut init provider unchanged. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/reboot.h>
#include <unistd.h>

static void fail(const char *message) {
        fprintf(stderr, "ISA_TEST_FAILED: %s\n", message);
        reboot(RB_POWER_OFF);
        for (;;)
                pause();
}

int main(int argc, char **argv, char **envp) {
        char next_init[4096];
        const char *marker = getenv("PREFLIGHT_MARKER");
        FILE *file;

        if (getpid() != 1 || argc != 2 || strcmp(argv[0], "/init") ||
            strcmp(argv[1], "test-argument") || !marker || strcmp(marker, "qemu-test"))
                fail("PID, argument vector, or environment was not preserved");
        if (access("/proc/self", F_OK) == 0)
                fail("proc was left mounted before handoff");
        file = fopen("/etc/isa-preflight-test/real-init", "r");
        if (!file || !fgets(next_init, sizeof(next_init), file))
                fail("could not read the original init path");
        fclose(file);
        next_init[strcspn(next_init, "\n")] = '\0';
        if (next_init[0] != '/')
                fail("original init path is not absolute");
        puts("ISA_TEST_HANDOFF: pid=1 argv0=/init argument=test-argument marker=qemu-test proc=unmounted");
        fflush(stdout);
        execve(next_init, argv, envp);
        fail("could not execute the original init provider");
}
