/* This file is part of isa-check.
 * Copyright (C) 2026 Canonical Ltd
 * SPDX-License-Identifier: GPL-3.0-only
 */
#ifndef ISA_PREFLIGHT_H
#define ISA_PREFLIGHT_H

#ifndef CONFIG_DIR
#define CONFIG_DIR "/etc/isa-preflight"
#endif
#define NEXT_INIT_PATH CONFIG_DIR "/next-init"

/* Shared parser I/O: both backends return negative errno values on failure. */
long preflight_sys_read(int fd, void *buffer, unsigned long count);
long preflight_sys_openat(const char *path);
long preflight_sys_close(int fd);

/* Additional init operations supplied by the freestanding x86-64 backend. */
long preflight_sys_write(int fd, const void *buffer, unsigned long count);
long preflight_sys_mkdir_proc(void);
long preflight_sys_mount_proc(void);
long preflight_sys_execve(const char *path, char *const argv[], char *const envp[]);
long preflight_sys_getpid(void);
long preflight_sys_reboot(unsigned long command);
long preflight_sys_pause(void);

/* Shared parsing helpers. First-line reading returns 1, 0 (EOF), or -1 (error). */
long preflight_read_first_line(const char *path, char *line, unsigned long capacity);
unsigned int cpu_check(const char *flags, char *missing, unsigned long missing_capacity);

/* Supplied by each executable: the CLI exits, while preflight halts. */
__attribute__((noreturn)) void preflight_fail(const char *message);

#endif
