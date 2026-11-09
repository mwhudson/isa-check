/* This file is part of isa-check.
 * Copyright (C) 2026 Canonical Ltd
 * SPDX-License-Identifier: GPL-3.0-only
 */
#ifndef ISA_PREFLIGHT_H
#define ISA_PREFLIGHT_H

/* Shared by both isa-preflight builds. */
#ifndef CONFIG_DIR
#define CONFIG_DIR "/etc/isa-preflight"
#endif
#define NEXT_INIT_PATH CONFIG_DIR "/next-init"
#define NEXT_INIT_SIZE 4096

/* Init operations supplied by the freestanding x86-64 backend. Each returns a
 * negative errno value on failure. */
#ifndef PROC_DIR
#define PROC_DIR "/proc"
#endif

long preflight_sys_write(int fd, const void *buffer, unsigned long count);
long preflight_sys_mkdir(const char *path, unsigned int mode);
long preflight_sys_mount_proc(const char *target);
long preflight_sys_umount(const char *target);
long preflight_sys_execve(const char *path, char *const argv[], char *const envp[]);
long preflight_sys_getpid(void);
long preflight_sys_reboot(unsigned long command);
long preflight_sys_pause(void);
__attribute__((noreturn)) void preflight_sys_exit(int status);

#endif
