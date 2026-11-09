/* This file is part of isa-check.
 * Copyright (C) 2026 Canonical Ltd
 * SPDX-License-Identifier: GPL-3.0-only
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "preflight.h"

#define DEFAULT_TABLE "/usr/share/dpkg/varianttable"
#define MAX_MISSING_FEATURES 512

__attribute__((noreturn)) void preflight_fail(const char *message) {
        fprintf(stderr, "isa-check: %s\n", message);
        exit(2);
}

static void usage(FILE *stream) {
        fprintf(stream, "Usage: isa-check [--table PATH] [VARIANT]\n");
}

int main(int argc, char **argv) {
        const char *table = DEFAULT_TABLE;
        const char *requested = NULL;
        char *line = NULL;
        size_t line_capacity = 0;
        unsigned int listed = 0;
        int found = 0;
        int status = 0;
        FILE *file;
        int i;

        for (i = 1; i < argc; i++) {
                if (!strcmp(argv[i], "--help")) {
                        usage(stdout);
                        return 0;
                }
                if (!strcmp(argv[i], "--table")) {
                        if (++i == argc)
                                preflight_fail("--table requires a path");
                        table = argv[i];
                        continue;
                }
                if (argv[i][0] == '-' || requested)
                        preflight_fail("usage: isa-check [--table PATH] [VARIANT]");
                requested = argv[i];
        }

        file = fopen(table, "r");
        if (!file)
                preflight_fail("could not open the variant table");

        while (getline(&line, &line_capacity, file) >= 0) {
                char *save = NULL;
                char *variant = strtok_r(line, " \t\r\n", &save);
                char *arch;
                char *flags;
                char missing[MAX_MISSING_FEATURES];

                if (!variant || variant[0] == '#')
                        continue;
                arch = strtok_r(NULL, " \t\r\n", &save);
                flags = strtok_r(NULL, " \t\r\n", &save);
                if (!arch || !flags)
                        preflight_fail("malformed row in the variant table");
                if (strcmp(arch, "amd64") || (requested && strcmp(requested, variant)))
                        continue;

                found = 1;
                if (cpu_check(flags, missing, sizeof(missing))) {
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
                preflight_fail("could not read the variant table");
        free(line);
        fclose(file);

        if (requested && !found)
                preflight_fail("variant is not present in the table");
        if (!requested && !listed)
                return 1;
        return status;
}
