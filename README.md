<!-- Copyright (C) 2026 Canonical Ltd -->
<!-- SPDX-License-Identifier: GPL-3.0-only -->

# isa-check and isa-preflight

Small tools for checking processor architecture variants:

- **isa-check** lists or probes compatible native-architecture variants using dpkg's
  `/usr/share/dpkg/varianttable` and the CPU flags reported in `/proc/cpuinfo`.
- **isa-preflight** is an initramfs entry point. Freestanding static variant
  builds check CPU requirements before handing off to userspace; dynamically
  linked non-variant builds simply chain-load the next init program. Variant
  checking currently supports x86-64.

## Build and test

Requires a C compiler, Linux headers, libc development files, Meson
0.63 or newer, Ninja, awk, binutils, Bash and coreutils. Unit tests use the
in-tree variant table; a variant build needs dpkg's table or an explicit override.

```sh
meson setup build --prefix=/usr
meson compile -C build
meson test -C build --print-errorlogs
```

The default build produces the non-variant chainloader. To embed amd64v3
requirements, configure a separate build with `-Dvariant=amd64v3`. Use
`-Dvarianttable=PATH` to select a different variant table.

For Debian/Ubuntu packages, install the build dependencies listed in
[`debian/control`](debian/control), then run:

```sh
dpkg-buildpackage -us -uc
```

Set `DEB_HOST_ARCH_VARIANT=amd64v3` for a variant package build.

## Probe CPU support

On amd64, with proc already mounted:

```sh
./build/isa-check          # List compatible variants in table order
./build/isa-check amd64v3  # Probe one variant
```

Exit status is **0** for support, **1** for incompatibility, or **2** for an
input/read error. `--table PATH` overrides the runtime variant table.
The native architecture is obtained from `dpkg --print-architecture`. CPU
features are read from Linux's `flags` (x86) or `Features` (ARM) fields. New
variant-table rows using those feature names need no CLI rebuild. Architectures
without table entries return an empty list with status 1.

## Initramfs handoff

The package installs a late dracut module as
`/usr/lib/dracut/modules.d/99preinit`. Installing or removing `isa-preflight`
therefore enables or disables the integration without changing dracut itself.
Installation, upgrades, and removal activate Ubuntu's `update-initramfs` trigger
so the configured initramfs generator refreshes existing images. The generator's
update policy controls which kernels are refreshed; images generated or managed
separately must be rebuilt by their image builder.
The module runs after dracut's init providers, installs `isa-preflight` as
`/init`, and writes `/etc/isa-preflight/next-init`. A symlinked init, such as
`init -> usr/lib/systemd/systemd`, is replaced with a handoff directly to its
resolved target. A regular init script is preserved as `/init.dracut`. With
`squash-lib`, only the outer loader is wrapped; the inner init is unchanged.
Absolute and relative symlink chains are resolved entirely inside the image,
including symlinks in parent directories.
The non-variant chainloader reuses the image's libc and dynamic loader. Dracut
installs its runtime dependencies into the same root as `/init`, including the
outer loader's root when squash is used.

Arguments and environment are preserved during handoff. The variant checker
mounts proc if needed, unmounts it again before the handoff, and runs with
baseline x86-64 instructions. The successor receives `argv[0]` unchanged
(normally `/init`). Errors halt the system when running as PID 1; otherwise
the program pauses indefinitely.

See [`isa-check(8)`](man/isa-check.8) and
[`isa-preflight(8)`](man/isa-preflight.8) for details. Licensing information is in
[`debian/copyright`](debian/copyright).
