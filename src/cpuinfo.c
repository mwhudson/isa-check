/* This file is part of isa-check.
 * Copyright (C) 2026 Canonical Ltd
 * SPDX-License-Identifier: GPL-3.0-only
 */
/* Shared, libc-independent parsing of cpuinfo and required feature lists.
 * The CLI and early-boot checker supply their own I/O backends. */
#include "cpuinfo.h"

#define READ_EOF 0
#define READ_LINE 1
#define READ_TRUNCATED 2

struct reader {
        long fd;
        char buffer[4096];
        unsigned long position;
        unsigned long length;
};

unsigned long isa_string_length(const char *string) {
        unsigned long length = 0;

        while (string[length])
                length++;
        return length;
}

/* Return READ_LINE, READ_EOF, or -1 for a read error. A line that does not fit
 * returns READ_TRUNCATED with as much as fits; the rest of it is discarded. */
static long read_line(struct reader *reader, char *line, unsigned long capacity) {
        unsigned long used = 0;
        int truncated = 0;

        for (;;) {
                char c;

                if (reader->position == reader->length) {
                        long result = isa_sys_read(reader->fd, reader->buffer, sizeof(reader->buffer));

                        if (result < 0)
                                return -1;
                        if (result == 0) {
                                if (used == 0 && !truncated)
                                        return READ_EOF;
                                line[used] = '\0';
                                return truncated ? READ_TRUNCATED : READ_LINE;
                        }
                        reader->length = (unsigned long) result;
                        reader->position = 0;
                }

                c = reader->buffer[reader->position++];
                if (c == '\n') {
                        line[used] = '\0';
                        return truncated ? READ_TRUNCATED : READ_LINE;
                }
                if (used + 1 < capacity)
                        line[used++] = c;
                else
                        truncated = 1;
        }
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

/* x86 calls the field "flags"; ARM uses "Features". Match the complete key
 * and leave colon validation to the caller so malformed fields are errors. */
static const char *flags_value(const char *line) {
        unsigned long length;

        if (has_prefix(line, "flags"))
                length = 5;
        else if (has_prefix(line, "Features"))
                length = 8;
        else
                return 0;
        return line[length] == ':' || is_space(line[length]) ? line + length : 0;
}

/* Find the next space-separated token at or after cursor, or return 0. */
static const char *next_token(const char *cursor, unsigned long *length) {
        const char *start;

        while (is_space(*cursor))
                cursor++;
        if (!*cursor)
                return 0;
        start = cursor;
        while (*cursor && !is_space(*cursor))
                cursor++;
        *length = (unsigned long) (cursor - start);
        return start;
}

static int has_token(const char *list, const char *name, unsigned long name_length) {
        const char *token = list;
        unsigned long length;

        while ((token = next_token(token, &length))) {
                if (length == name_length) {
                        unsigned long i = 0;

                        while (i < length && token[i] == name[i])
                                i++;
                        if (i == length)
                                return 1;
                }
                token += length;
        }
        return 0;
}

/* Store the tokens of value in flags, separated by single spaces. */
static int copy_tokens(char *flags, unsigned long capacity, const char *value) {
        const char *token = value;
        unsigned long used = 0;
        unsigned long length;

        while ((token = next_token(token, &length))) {
                unsigned long i;

                if (used + (used ? 1 : 0) + length >= capacity)
                        return CPU_ERROR_TOO_LONG;
                if (used)
                        flags[used++] = ' ';
                for (i = 0; i < length; i++)
                        flags[used++] = token[i];
                token += length;
        }
        flags[used] = '\0';
        return 0;
}

/* Remove, in place, every token of flags that value does not also contain. */
static void keep_common(char *flags, const char *value) {
        const char *token = flags;
        char *out = flags;
        unsigned long length;

        while ((token = next_token(token, &length))) {
                if (has_token(value, token, length)) {
                        unsigned long i;

                        /* out never passes token, so copying forwards is safe. */
                        if (out != flags)
                                *out++ = ' ';
                        for (i = 0; i < length; i++)
                                *out++ = token[i];
                }
                token += length;
        }
        *out = '\0';
}

int cpu_read_flags(char *flags, unsigned long capacity) {
        char line[CPU_FLAGS_SIZE];
        struct reader reader;
        unsigned long processors = 0;
        int error = 0;
        long result;

        if (capacity == 0)
                return CPU_ERROR_TOO_LONG;
        flags[0] = '\0';

        reader.fd = isa_sys_open_readonly(CPUINFO_PATH);
        if (reader.fd < 0)
                return CPU_ERROR_OPEN;
        reader.position = 0;
        reader.length = 0;

        while ((result = read_line(&reader, line, sizeof(line))) > 0) {
                const char *value = flags_value(line);

                /* Other lines are irrelevant, even if they were too long to read whole. */
                if (!value)
                        continue;
                if (result == READ_TRUNCATED) {
                        error = CPU_ERROR_TOO_LONG;
                        break;
                }
                while (is_space(*value))
                        value++;
                if (*value != ':') {
                        error = CPU_ERROR_MALFORMED;
                        break;
                }
                value++;

                if (processors++ == 0)
                        error = copy_tokens(flags, capacity, value);
                else
                        keep_common(flags, value);
                if (error)
                        break;
        }
        isa_sys_close((int) reader.fd);

        if (error)
                return error;
        if (result < 0)
                return CPU_ERROR_READ;
        if (processors == 0)
                return CPU_ERROR_NO_FLAGS;
        return 0;
}

/* Append text to the list, which must have room for it. */
static void append_text(char *missing, unsigned long *used, const char *text, unsigned long length) {
        unsigned long i;

        if (*used) {
                missing[(*used)++] = ',';
                missing[(*used)++] = ' ';
        }
        for (i = 0; i < length; i++)
                missing[(*used)++] = text[i];
        missing[*used] = '\0';
}

/* Append a feature to the list. Room is always kept for a final ", ...", which
 * replaces the first feature that does not fit and everything after it. */
static void append_missing(char *missing, unsigned long capacity, int *truncated, const char *feature,
                           unsigned long length) {
        unsigned long used = isa_string_length(missing);

        if (*truncated)
                return;
        if (used + (used ? 2 : 0) + length + 5 < capacity) {
                append_text(missing, &used, feature, length);
        } else if (used + (used ? 2 : 0) + 3 < capacity) {
                append_text(missing, &used, "...", 3);
                *truncated = 1;
        }
}

int cpu_missing_features(const char *flags, const char *required, char *missing, unsigned long missing_capacity) {
        const char *start = required;
        int missing_count = 0;
        int truncated = 0;

        if (missing_capacity == 0)
                return CPU_ERROR_INVALID_REQUIREMENTS;
        missing[0] = '\0';

        for (;;) {
                const char *end = start;

                while (*end && *end != ',')
                        end++;
                if (end == start)
                        return CPU_ERROR_INVALID_REQUIREMENTS;
                if (!has_token(flags, start, (unsigned long) (end - start))) {
                        append_missing(missing, missing_capacity, &truncated, start, (unsigned long) (end - start));
                        missing_count++;
                }
                if (!*end)
                        return missing_count;
                start = end + 1;
        }
}

const char *cpu_error_message(int error) {
        switch (error) {
        case CPU_ERROR_OPEN:
                return "could not open " CPUINFO_PATH;
        case CPU_ERROR_READ:
                return "could not read " CPUINFO_PATH;
        case CPU_ERROR_NO_FLAGS:
                return "no processor flags found in " CPUINFO_PATH;
        case CPU_ERROR_MALFORMED:
                return "malformed processor flags in " CPUINFO_PATH;
        case CPU_ERROR_TOO_LONG:
                return "processor flags in " CPUINFO_PATH " are too long";
        case CPU_ERROR_INVALID_REQUIREMENTS:
                return "invalid required CPU feature list";
        default:
                return "unknown CPU feature check error";
        }
}
