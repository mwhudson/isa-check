/* This file is part of isa-check.
 * Copyright (C) 2026 Canonical Ltd
 * SPDX-License-Identifier: GPL-3.0-only
 */
/* Command-line frontend: read dpkg's variant table and list or probe variants
 * against the CPU features shared by all processors reported by the kernel. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cpuinfo.h"

#define DEFAULT_TABLE "/usr/share/dpkg/varianttable"
#define USAGE "Usage: isa-check [--table PATH] [VARIANT]\n"

static char cpu_flags[CPU_FLAGS_SIZE];

__attribute__((noreturn)) static void fail(const char *message) {
        fprintf(stderr, "isa-check: %s\n", message);
        exit(2);
}

/* Ask dpkg at runtime so variant selection follows the native userspace
 * architecture, including when the binary was cross-built. */
static void read_architecture(char *architecture, size_t capacity) {
        FILE *file = popen("dpkg --print-architecture", "r");
        int have_line;
        int status;

        if (!file)
                fail("could not determine the native architecture using dpkg");
        have_line = fgets(architecture, capacity, file) != NULL;
        status = pclose(file);
        if (!have_line || status || !strchr(architecture, '\n'))
                fail("could not determine the native architecture using dpkg");
        architecture[strcspn(architecture, "\r\n")] = '\0';
        if (!architecture[0] || strspn(architecture, "abcdefghijklmnopqrstuvwxyz0123456789-") != strlen(architecture))
                fail("dpkg reported an invalid native architecture");
}

int main(int argc, char **argv) {
        const char *table = DEFAULT_TABLE;
        const char *requested = NULL;
        char *line = NULL;
        char architecture[64];
        size_t line_capacity = 0;
        unsigned int listed = 0;
        int have_flags = 0;
        int found = 0;
        int status = 0;
        int options = 1;
        FILE *file;
        int i;

        for (i = 1; i < argc; i++) {
                if (options && (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h"))) {
                        fputs(USAGE, stdout);
                        return 0;
                }
                if (options && !strcmp(argv[i], "--table")) {
                        if (++i == argc)
                                fail("--table requires a path");
                        table = argv[i];
                        continue;
                }
                if (options && !strcmp(argv[i], "--")) {
                        options = 0;
                        continue;
                }
                if ((options && argv[i][0] == '-') || requested) {
                        fputs(USAGE, stderr);
                        exit(2);
                }
                requested = argv[i];
        }

        file = fopen(table, "r");
        if (!file)
                fail("could not open the variant table");
        read_architecture(architecture, sizeof(architecture));

        while (getline(&line, &line_capacity, file) >= 0) {
                char *save = NULL;
                char *variant = strtok_r(line, " \t\r\n", &save);
                char *arch;
                char *flags;
                char missing[MISSING_FEATURES_SIZE];
                int result;

                if (!variant || variant[0] == '#')
                        continue;
                arch = strtok_r(NULL, " \t\r\n", &save);
                flags = strtok_r(NULL, " \t\r\n", &save);
                if (!arch || !flags)
                        fail("malformed row in the variant table");
                if (strcmp(arch, architecture) || (requested && strcmp(requested, variant)))
                        continue;

                if (!have_flags) {
                        result = cpu_read_flags(cpu_flags, sizeof(cpu_flags));
                        if (result < 0)
                                fail(cpu_error_message(result));
                        have_flags = 1;
                }

                found = 1;
                result = cpu_missing_features(cpu_flags, flags, missing, sizeof(missing));
                if (result < 0)
                        fail(cpu_error_message(result));
                if (result) {
                        if (requested) {
                                fprintf(stderr, "%s is not supported: %s\n", variant, missing);
                                status = 1;
                                break;
                        }
                        continue;
                }

                if (requested)
                        break;
                puts(variant);
                listed++;
        }
        if (ferror(file))
                fail("could not read the variant table");
        free(line);
        fclose(file);

        if (fflush(stdout) || ferror(stdout))
                fail("could not write output");
        if (requested && !found)
                fail("variant is not present in the table");
        if (!requested && !listed)
                return 1;
        return status;
}
