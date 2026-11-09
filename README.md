<!-- Copyright (C) 2026 Canonical Ltd -->
<!-- SPDX-License-Identifier: GPL-3.0-only -->

# isa-check and isa-preflight

Small tools for checking processor architecture variants:

- **isa-check** lists or probes compatible amd64 variants using dpkg's
  `/usr/share/dpkg/varianttable` and the CPU flags reported in `/proc/cpuinfo`.
- **isa-preflight** is a static initramfs entry point. Variant builds check CPU
  requirements before handing off to userspace; non-variant builds simply
  chain-load the next init program. Variant checking currently supports x86-64.

## Build and test

Requires a C compiler, Linux headers, static libc development files, Meson
0.63 or newer, Ninja, awk, binutils, Bash and coreutils. On x86-64, dpkg's
variant table is also needed for the tests.

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

## Initramfs handoff

The image builder installs `isa-preflight` as `/init` and provides
`/etc/isa-preflight/next-init`: one line containing the absolute path of the
successor executable. Arguments and environment are preserved during handoff.
The variant checker mounts proc if needed and runs with baseline x86-64
instructions. Errors halt the system when running as PID 1; otherwise the
program pauses indefinitely.

See [`isa-check(8)`](man/isa-check.8) and
[`isa-preflight(8)`](man/isa-preflight.8) for details. Licensing information is in
[`debian/copyright`](debian/copyright).
