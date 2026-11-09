/* This file is part of isa-check.
 * Copyright (C) 2026 Canonical Ltd
 * SPDX-License-Identifier: GPL-3.0-only
 */
#include "preflight.h"

#ifndef CPUINFO_PATH
#define CPUINFO_PATH "/proc/cpuinfo"
#endif
#define MAX_FEATURES 64
#define MAX_FLAGS_DATA 2048
#define LINE_SIZE 4096

struct reader {
        long fd;
        char buffer[4096];
        unsigned long position;
        unsigned long length;
};

static unsigned long string_length(const char *string) {
        unsigned long length = 0;

        while (string[length])
                length++;
        return length;
}

/* Return 1 for a line (including empty), 0 for EOF, or -1 for a read/size error. */
static long read_line(struct reader *reader, char *line, unsigned long capacity) {
        unsigned long used = 0;

        for (;;) {
                char c;

                if (reader->position == reader->length) {
                        long result = preflight_sys_read(reader->fd, reader->buffer, sizeof(reader->buffer));

                        if (result < 0)
                                return -1;
                        if (result == 0) {
                                if (used == 0)
                                        return 0;
                                line[used] = '\0';
                                return 1;
                        }
                        reader->length = (unsigned long) result;
                        reader->position = 0;
                }

                if (used + 1 >= capacity)
                        return -1;

                c = reader->buffer[reader->position++];
                if (c == '\n') {
                        line[used] = '\0';
                        return 1;
                }
                line[used++] = c;
        }
}

long preflight_read_first_line(const char *path, char *line, unsigned long capacity) {
        struct reader reader;
        long result;

        reader.fd = preflight_sys_openat(path);
        if (reader.fd < 0)
                return -1;
        reader.position = 0;
        reader.length = 0;

        result = read_line(&reader, line, capacity);
        preflight_sys_close((int) reader.fd);
        return result;
}

static void append_missing(char *missing, unsigned long capacity, const char *feature) {
        unsigned long used = string_length(missing);

        if (used && used + 2 < capacity) {
                missing[used++] = ',';
                missing[used++] = ' ';
        }
        while (*feature && used + 1 < capacity)
                missing[used++] = *feature++;
        missing[used] = '\0';
}

static unsigned long load_requirements(const char *flags, char *flags_data, char **required_features) {
        unsigned long length = string_length(flags);
        unsigned long required_count = 0;
        unsigned long i;
        char *cursor;
        char *start;

        if (length == 0 || length >= MAX_FLAGS_DATA)
                preflight_fail("invalid required CPU feature list");
        for (i = 0; i < length; i++)
                flags_data[i] = flags[i];
        flags_data[length] = '\0';

        start = flags_data;
        for (cursor = flags_data;; cursor++) {
                if (*cursor && *cursor != ',')
                        continue;
                if (cursor == start || required_count == MAX_FEATURES)
                        preflight_fail("invalid required CPU feature list");
                required_features[required_count++] = start;
                if (!*cursor)
                        break;
                *cursor = '\0';
                start = cursor + 1;
        }
        return required_count;
}

static int has_prefix(const char *string, const char *prefix) {
        while (*prefix) {
                if (*string++ != *prefix++)
                        return 0;
        }
        return 1;
}

static int is_space(char c) {
        return c == ' ' || c == '\t' || c == '\r';
}

static int has_flag(const char *flags, const char *required) {
        const char *cursor = flags;

        while (*cursor) {
                const char *start;
                const char *name;

                while (is_space(*cursor))
                        cursor++;
                if (!*cursor)
                        break;
                start = cursor;
                while (*cursor && !is_space(*cursor))
                        cursor++;

                name = required;
                while (start < cursor && *name && start[0] == name[0]) {
                        start++;
                        name++;
                }
                if (start == cursor && !*name)
                        return 1;
        }
        return 0;
}

static void check_processor(const char *line, char **required_features, unsigned char *missing_features,
                            unsigned long required_count) {
        const char *flags = line;
        unsigned long i;

        while (*flags && *flags != ':')
                flags++;
        if (!*flags)
                preflight_fail("malformed processor flags in " CPUINFO_PATH);
        flags++;

        for (i = 0; i < required_count; i++) {
                if (!missing_features[i] && !has_flag(flags, required_features[i]))
                        missing_features[i] = 1;
        }
}

unsigned int cpu_check(const char *flags, char *missing, unsigned long missing_capacity) {
        char flags_data[MAX_FLAGS_DATA];
        char *required_features[MAX_FEATURES];
        unsigned char missing_features[MAX_FEATURES];
        unsigned long required_count;
        struct reader reader;
        char line[LINE_SIZE];
        unsigned int missing_count = 0;
        unsigned long processors = 0;
        unsigned long i;
        long result;

        if (missing_capacity == 0)
                preflight_fail("invalid missing feature buffer");
        missing[0] = '\0';
        for (i = 0; i < MAX_FEATURES; i++)
                missing_features[i] = 0;
        required_count = load_requirements(flags, flags_data, required_features);

        reader.fd = preflight_sys_openat(CPUINFO_PATH);
        if (reader.fd < 0)
                preflight_fail("could not open " CPUINFO_PATH);
        reader.position = 0;
        reader.length = 0;

        while ((result = read_line(&reader, line, sizeof(line))) > 0) {
                if (!has_prefix(line, "flags\t"))
                        continue;
                processors++;
                check_processor(line, required_features, missing_features, required_count);
        }
        preflight_sys_close((int) reader.fd);

        if (result < 0)
                preflight_fail("could not read " CPUINFO_PATH);
        if (processors == 0)
                preflight_fail("no processor flags found in " CPUINFO_PATH);

        for (i = 0; i < required_count; i++) {
                if (missing_features[i]) {
                        append_missing(missing, missing_capacity, required_features[i]);
                        missing_count++;
                }
        }
        return missing_count;
}
