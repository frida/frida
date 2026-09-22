#!/usr/bin/env python3

import argparse
import json
import os
import re
from pathlib import Path
import shlex
import shutil
import socket
import subprocess
import threading
import sys
import tempfile
import time
from typing import NamedTuple
import urllib.request


def main():
    if len(sys.argv) >= 2 and sys.argv[1].split("-", maxsplit=1)[0] in ("check", "boot", "rewind",
                                                                       "live", "livecheck"):
        parser = argparse.ArgumentParser()
        subparsers = parser.add_subparsers(dest="command", required=True)

        for arch in GUESTS:
            subparsers.add_parser(f"check-{arch}",
                                  help=f"verify that the {arch} guest can be booted, fetching it if needed")

            boot = subparsers.add_parser(f"boot-{arch}",
                                         help=f"boot a minimal {arch} guest with the GDB stub enabled")
            boot.add_argument("--gdb-port", type=int, required=True)
            boot.add_argument("--memory", type=int, default=256)

            subparsers.add_parser(f"livecheck-{arch}",
                                  help=f"verify that a live {arch} guest can be booted, fetching what it needs")

            live = subparsers.add_parser(f"live-{arch}",
                                         help=f"boot a live {arch} guest with userspace, the GDB stub and a "
                                              "hostlink")
            live.add_argument("--gdb-port", type=int, required=True)
            live.add_argument("--memory", type=int, default=256)
            live.add_argument("--socket", type=Path, required=True)
            live.add_argument("--qmp", type=Path, required=True)
            live.add_argument("--console-commands", type=Path)

        for name, guest in WINDOWS_GUESTS.items():
            check = subparsers.add_parser(f"check-{name}",
                                          help=f"verify that the {guest.description} guest can be booted")
            check.add_argument("--image", type=Path, required=True)
            check.add_argument("--format", default="qcow2")

            windows = subparsers.add_parser(f"boot-{name}",
                                            help=f"boot a {guest.description} disk image with the GDB stub enabled")
            windows.add_argument("--image", type=Path, required=True)
            windows.add_argument("--format", default="qcow2")
            windows.add_argument("--disk-interface", choices=["ide", "lsi"], default=guest.disk_interface)
            windows.add_argument("--gdb-port", type=int, required=True)
            windows.add_argument("--memory", type=int, default=guest.memory)
            windows.add_argument("--boot-seconds", type=int, default=guest.boot_seconds)
            windows.add_argument("--qmp", type=Path)
            windows.add_argument("--debugcon", type=Path)
            windows.add_argument("--snapshot", default=SNAPSHOT_NAME)

            rewind = subparsers.add_parser(f"rewind-{name}",
                                           help=f"restore a {guest.description} snapshot and put a fresh "
                                                "hostlink port on it")
            rewind.add_argument("--qmp", type=Path, required=True)
            rewind.add_argument("--snapshot", default=SNAPSHOT_NAME)

        subparsers.add_parser("check-kmod",
                              help="verify that a kmod guest can be booted, fetching what it needs")

        boot_kmod = subparsers.add_parser("boot-kmod",
                                          help="boot the running kernel in a guest and load a module into it")
        boot_kmod.add_argument("--module", type=Path, required=True)
        boot_kmod.add_argument("--kernel", type=Path)
        boot_kmod.add_argument("--memory", type=int, default=512)
        boot_kmod.add_argument("--cpus", type=int, default=4)
        boot_kmod.add_argument("--socket", type=Path)
        boot_kmod.add_argument("--ibt", action=argparse.BooleanOptionalAction, default=True)

        args = parser.parse_args()
        action, target = args.command.split("-", maxsplit=1)
        if target == "kmod":
            if action == "check":
                check_kmod_guest()
            else:
                boot_kmod_guest(args)
        elif target in WINDOWS_GUESTS:
            if action == "check":
                check_windows_guest(WINDOWS_GUESTS[target], args)
            elif action == "rewind":
                rewind_windows_guest(args)
            else:
                boot_windows_guest(WINDOWS_GUESTS[target], args)
        elif action == "livecheck":
            check_live_guest(target)
        elif action == "live":
            boot_live_guest(target, args)
        elif action == "check":
            check_guest(target)
        else:
            boot_guest(target, args)
        return

    arch = sys.argv[1]
    args = sys.argv[2:] if len(sys.argv) >= 3 else []
    run(arch, args)


def run(arch: str, args: [str]):
    import pexpect

    child = pexpect.spawn("arm_now", ["start", arch, "--sync"])

    child.expect("buildroot login: ")
    child.sendline("root")
    child.expect("# ")

    child.sendline(shlex.join(["/root/frida-tests"] + args))
    child.interact()


def check_guest(arch: str):
    """
    Do everything booting needs except the boot itself, so that a caller can tell a missing
    prerequisite apart from a guest that failed to come up — and pay for the download here
    rather than inside its connect timeout.
    """
    guest = GUESTS[arch]
    require_qemu(guest)
    fetch_kernel(guest)
    print("ok")


def boot_guest(arch: str, args):
    """
    Boot a stock 32-bit Linux kernel far enough to have its page tables up, and hand it to
    QEMU's GDB stub. There is no root filesystem, so the kernel panics once it goes looking
    for one; panic=0 then parks it forever with paging still enabled, which is exactly the
    quiescent state we want to inspect.

    QEMU discards every GDB packet except Ctrl-C while the guest is running, so the caller
    cannot attach and then wait for the boot — it has to wait first. Watch the serial console
    on its behalf and announce readiness once the kernel has parked itself.
    """
    guest = GUESTS[arch]
    qemu = require_qemu(guest)
    kernel = fetch_kernel(guest)

    machine_arguments = []
    if guest.machine != "":
        machine_arguments += ["-machine", guest.machine]
    if guest.cpu != "":
        machine_arguments += ["-cpu", guest.cpu]

    process = subprocess.Popen([
        qemu,
        "-m", str(args.memory),
    ] + machine_arguments + [
        "-kernel", str(kernel),
        "-append", f"console={guest.console} panic=0",
        "-display", "none",
        "-serial", "stdio",
        "-monitor", "none",
        "-no-reboot",
        "-gdb", f"tcp::{args.gdb_port}",
    ], stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)

    try:
        for raw_line in process.stdout:
            line = raw_line.decode(errors="replace").rstrip()
            print(line, file=sys.stderr)
            if BOOT_COMPLETE_MARKER in line:
                print("ready", flush=True)
                process.wait()
                return

        raise Unavailable("guest exited before it finished booting")
    finally:
        process.kill()


def check_live_guest(arch: str):
    guest = GUESTS[arch]
    if guest.ramdisk_url == "":
        raise Unavailable(f"no ramdisk is published for {arch}")
    require_qemu(guest)
    fetch_kernel(guest)
    fetch_ramdisk(guest)
    fetch_system_map(guest)
    print("ok")


def boot_live_guest(arch: str, args):
    """
    Boot the same stock kernel the quiescent guest uses, but hand it the ramdisk the
    distribution ships beside it: an injected agent needs a scheduler to run on and processes
    to find, neither of which a kernel parked on a panic has. Nothing is there for that
    ramdisk to mount, so it drops to its recovery shell, which is userspace enough.

    The GDB stub puts the agent in and the hostlink carries its RPC. What the guest cannot be
    asked for goes out on stdout ahead of readiness: the System.map naming the kernel's
    symbols, and where its configuration space is mapped.
    """
    guest = GUESTS[arch]
    qemu = require_qemu(guest)
    kernel = fetch_kernel(guest)
    ramdisk = fetch_ramdisk(guest)
    system_map = fetch_system_map(guest)

    machine_arguments = []
    if guest.live_machine != "":
        machine_arguments += ["-machine", guest.live_machine]
    if guest.cpu != "":
        machine_arguments += ["-cpu", guest.cpu]

    process = subprocess.Popen([
        qemu,
        "-m", str(args.memory),
    ] + machine_arguments + [
        "-kernel", str(kernel),
        "-initrd", str(ramdisk),
        "-append", f"console={guest.console} panic=-1 {guest.live_cmdline}".rstrip(),
        "-display", "none",
        "-serial", "stdio",
        "-monitor", "none",
        "-no-reboot",
        "-qmp", f"unix:{args.qmp},server=on,wait=off",
        "-gdb", f"tcp::{args.gdb_port}",
    ] + hostlink_arguments(guest),
        stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)

    if args.console_commands is not None:
        threading.Thread(target=feed_console, args=(args.console_commands, process), daemon=True).start()

    asked = False
    ready = False
    where_it_is = []
    try:
        for raw_line in process.stdout:
            line = raw_line.decode(errors="replace").rstrip()
            print(line, file=sys.stderr)

            if not asked and LIVE_READY_MARKER in line:
                asked = True
                if guest.hostlink == "mmio":
                    process.stdin.write((WHERE_THE_LINK_IS + "\n").encode())
                else:
                    process.stdin.write((LET_THE_LINK_GO + "\n").encode())
                process.stdin.flush()

            if not ready and LINK_ANSWER_MARKER in line and LINK_GONE_MARKER not in line:
                fields = line.split(LINK_ANSWER_MARKER, maxsplit=1)[1].split()
                where_it_is = [f"mmio 0x{fields[0]}", f"irq {fields[1]}"]

                process.stdin.write((LET_THE_LINK_GO + "\n").encode())
                process.stdin.flush()

            if not ready and LINK_GONE_MARKER in line:
                ready = True

                print(f"system-map {system_map}", flush=True)
                for detail in where_it_is:
                    print(detail, flush=True)
                print(f"bus {VIRTIO_SERIAL_ID}.0", flush=True)
                print("ready", flush=True)

        if not ready:
            raise Unavailable("guest exited before it said where the link is")
    finally:
        process.kill()


def hostlink_arguments(guest: "Guest") -> [str]:
    if guest.hostlink == "mmio":
        return [
            "-global", "virtio-mmio.force-legacy=false",
            "-device", f"virtio-serial-device,id={VIRTIO_SERIAL_ID}",
        ]
    return ["-device", f"virtio-serial-pci,id={VIRTIO_SERIAL_ID}"]


def feed_console(path: Path, process):
    while process.poll() is None:
        try:
            with open(path, "r") as commands:
                for command in commands:
                    process.stdin.write(command.encode())
                    process.stdin.flush()
        except Exception:
            return


def check_windows_guest(guest: "WindowsGuest", args):
    require_windows_guest(guest, args)
    print("ok")


def boot_windows_guest(guest: "WindowsGuest", args):
    """
    Boot a Windows disk image and hand it to QEMU's GDB stub.

    These guests say nothing on the serial line, so there is no marker to wait for the way the
    Linux guests provide one. Give the boot a fixed budget instead and let the caller decide
    whether what it finds looks like a kernel with its page tables up.

    TCG rather than KVM: the stub is well-behaved there, and it sidesteps the divide overflow
    that fast processors provoke in Windows 95.
    """
    qemu = require_windows_guest(guest, args)

    control = []
    if args.qmp is not None:
        control = [
            "-qmp", f"unix:{args.qmp},server=on,wait=off",
            "-device", "virtio-serial-pci,id=" + VIRTIO_SERIAL_ID,
        ]

    diagnostics = []
    if args.debugcon is not None:
        diagnostics = [
            "-chardev", f"file,id={DEBUGCON_ID},path={args.debugcon}",
            "-device", f"isa-debugcon,iobase={DEBUGCON_PORT:#x},chardev={DEBUGCON_ID}",
        ]

    process = subprocess.Popen([
        qemu,
        "-machine", "pc,accel=tcg",
        "-cpu", guest.cpu,
        "-m", str(args.memory),
    ] + disk_arguments(args) + [
        "-vga", "cirrus",
        "-nic", "none",
        "-display", "none",
        "-monitor", "none",
        "-no-shutdown",
        "-chardev", f"socket,id={GDB_ID},host=127.0.0.1,port={args.gdb_port},server=on,wait=off",
        "-gdb", f"chardev:{GDB_ID}",
    ] + control + diagnostics, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    try:
        if args.qmp is None:
            for _ in range(args.boot_seconds):
                if process.poll() is not None:
                    raise Unavailable("guest exited before it finished booting")
                time.sleep(1)
        elif guest.settle is not None:
            guest.settle(Qmp(args.qmp, process), args.snapshot)
        else:
            for _ in range(args.boot_seconds):
                if process.poll() is not None:
                    raise Unavailable("guest exited before it finished booting")
                time.sleep(1)

        print("ready", flush=True)
        process.wait()
    finally:
        process.kill()


# Windows 95 says nothing on the serial line, so readiness is a thing to look at rather than
# wait out: the mode change out of text is the shell coming up, and the adapter it cannot find
# leaves a dialog over the desktop that has to go before the machine is worth snapshotting.
def settle_win95_desktop(qmp, snapshot):
    deadline = time.monotonic() + WIN95_BOOT_TIMEOUT_SECONDS
    while qmp.screen_size() == TEXT_MODE_SIZE:
        if time.monotonic() > deadline:
            raise Unavailable("guest never left text mode")
        time.sleep(WIN95_BOOT_POLL_SECONDS)

    time.sleep(WIN95_SHELL_SETTLE_SECONDS)
    qmp.monitor("sendkey ret")
    time.sleep(WIN95_DIALOG_SETTLE_SECONDS)
    qmp.monitor("sendkey alt-f4")
    time.sleep(WIN95_DIALOG_SETTLE_SECONDS)

    qmp.monitor("stop")
    qmp.monitor(f"savevm {snapshot}")
    qmp.monitor("cont")


# The snapshot is of a machine that never had a hostlink port, and the port a previous run added
# goes away asynchronously: restoring before it is gone leaves QEMU unable to load the device's
# state, and every rewind after that runs a wedged machine.
def rewind_windows_guest(args):
    qmp = Qmp(args.qmp)

    qmp.execute("device_del", {"id": PORT_DEVICE_ID})
    deadline = time.monotonic() + PORT_REMOVAL_TIMEOUT_SECONDS
    while PORT_DEVICE_ID in qmp.peripherals():
        if time.monotonic() > deadline:
            raise Unavailable("hostlink port would not go away")
        time.sleep(PORT_REMOVAL_POLL_SECONDS)

    # A run leaves its chardev behind, and removing one takes a moment to land. On the first
    # rewind after a boot there is none, which is why this asks rather than reads the error.
    while PORT_CHARDEV_ID in qmp.chardevs():
        qmp.execute("chardev-remove", {"id": PORT_CHARDEV_ID})
        if time.monotonic() > deadline:
            raise Unavailable("hostlink chardev would not go away")
        time.sleep(PORT_REMOVAL_POLL_SECONDS)

    reply = qmp.monitor(f"loadvm {args.snapshot}")
    if reply:
        raise Unavailable(f"unable to restore {args.snapshot}: {reply}")
    qmp.monitor("cont")


class Qmp:
    def __init__(self, path, process=None):
        deadline = time.monotonic() + QMP_CONNECT_TIMEOUT_SECONDS
        while True:
            if process is not None and process.poll() is not None:
                raise Unavailable("guest exited before it finished booting")
            try:
                self._sock = socket.socket(socket.AF_UNIX)
                self._sock.connect(str(path))
                break
            except OSError:
                if time.monotonic() > deadline:
                    raise Unavailable("guest never opened its QMP socket")
                time.sleep(QMP_CONNECT_POLL_SECONDS)

        self._conn = self._sock.makefile("rw")
        self._conn.readline()
        self.execute("qmp_capabilities")

    def execute(self, command, arguments=None):
        message = {"execute": command}
        if arguments is not None:
            message["arguments"] = arguments
        self._conn.write(json.dumps(message) + "\n")
        self._conn.flush()
        while True:
            reply = json.loads(self._conn.readline())
            if "event" not in reply:
                return reply

    def monitor(self, command):
        return self.execute("human-monitor-command", {"command-line": command})["return"].strip()

    def peripherals(self):
        return [entry["name"] for entry in self.execute("qom-list", {"path": "/machine/peripheral"})["return"]]

    def chardevs(self):
        return [entry["label"] for entry in self.execute("query-chardev")["return"]]

    def screen_size(self):
        with tempfile.NamedTemporaryFile(suffix=".ppm") as shot:
            self.execute("screendump", {"filename": shot.name})
            header = open(shot.name, "rb").read(32).split()
            return (int(header[1]), int(header[2]))


# Images exported from VMware put the system disk on a SCSI adapter, which the guest's own
# driver expects to find; handing it over as IDE bugchecks the kernel.
def disk_arguments(args) -> [str]:
    drive = f"file={args.image},format={args.format}"
    if args.disk_interface == "ide":
        return ["-drive", drive + ",if=ide"]
    return [
        "-drive", drive + ",if=none,id=disk",
        "-device", "lsi53c895a,id=scsi0",
        "-device", "scsi-hd,drive=disk,bus=scsi0.0",
    ]


def require_windows_guest(guest: "WindowsGuest", args) -> str:
    qemu = shutil.which(guest.emulator)
    if qemu is None:
        raise Unavailable(f"{guest.emulator} is not installed")
    if not args.image.exists():
        raise Unavailable(f"{args.image} does not exist")
    return qemu


def check_kmod_guest():
    guest = GUESTS[KMOD_GUEST_ARCH]
    require_qemu(guest)
    require_program("cpio")
    fetch_busybox(guest)

    kernel = host_kernel()
    if not os.access(kernel, os.R_OK):
        raise Unavailable(f"{kernel} is not readable")

    print("ok")


def boot_kmod_guest(args):
    guest = GUESTS[KMOD_GUEST_ARCH]
    qemu = require_qemu(guest)
    busybox = fetch_busybox(guest)
    kernel = args.kernel if args.kernel is not None else host_kernel()

    cmdline = ["console=ttyS0", "panic=-1"]
    if not args.ibt:
        cmdline.append("ibt=off")

    port = PORT_NAME if args.socket is not None else None

    with tempfile.TemporaryDirectory() as staging_dir:
        initramfs = build_initramfs(Path(staging_dir), busybox, args.module, port)

        channel = []
        if args.socket is not None:
            channel = [
                "-device", "virtio-serial",
                "-chardev", f"socket,id=frida,path={args.socket},server=on,wait=off",
                "-device", f"virtserialport,chardev=frida,name={port}",
            ]

        process = subprocess.Popen([
            qemu,
            "-m", str(args.memory),
            "-smp", str(args.cpus),
            "-enable-kvm",
            "-cpu", "host",
            "-kernel", str(kernel),
            "-initrd", str(initramfs),
            "-append", " ".join(cmdline),
            "-display", "none",
            "-serial", "stdio",
            "-monitor", "none",
            "-no-reboot",
        ] + channel, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)

        listening = False
        try:
            for raw_line in process.stdout:
                line = raw_line.decode(errors="replace").rstrip()
                print(line, file=sys.stderr)
                if MODULE_LOAD_FAILED_MARKER in line:
                    raise Unavailable("the module failed to load")
                if SLEPT_WHERE_FORBIDDEN_MARKER in line:
                    raise Unavailable("the module slept where the caller forbids it")
                if not listening and AGENT_READY_MARKER in line:
                    print("ready", flush=True)
                    listening = True

            if not listening:
                raise Unavailable("guest exited before the module was listening")
        finally:
            process.kill()


def require_qemu(guest: "Guest") -> str:
    qemu = shutil.which(guest.qemu_binary)
    if qemu is None:
        raise Unavailable(f"{guest.qemu_binary} is not installed")
    return qemu


def fetch_kernel(guest: "Guest") -> Path:
    return fetch_cached(guest, "vmlinuz-lts", guest.kernel_url)


def require_program(name: str) -> str:
    program = shutil.which(name)
    if program is None:
        raise Unavailable(f"{name} is not installed")
    return program


def fetch_busybox(guest: "Guest") -> Path:
    return fetch_cached(guest, "busybox", BUSYBOX_URL)


def fetch_ramdisk(guest: "Guest") -> Path:
    return fetch_cached(guest, "initramfs-lts", guest.ramdisk_url)


def fetch_system_map(guest: "Guest") -> Path:
    """
    The map is named after the kernel it describes, which the mirror versions in place, so
    the directory says which one is being served today.
    """
    netboot = guest.kernel_url.rsplit("/", maxsplit=1)[0] + "/"
    try:
        with urllib.request.urlopen(netboot, timeout=60) as response:
            index = response.read().decode(errors="replace")
    except Exception as e:
        raise Unavailable(f"unable to list {netboot}: {e}")

    names = sorted(set(re.findall(r'System\.map-[0-9][^"<]*-lts', index)))
    if not names:
        raise Unavailable(f"{netboot} serves no System.map")

    return fetch_cached(guest, names[-1], netboot + names[-1])


def fetch_system_map(guest: "Guest") -> Path:
    """
    The map is named after the kernel it describes, which the mirror versions in place, so
    the directory says which one is being served today.
    """
    netboot = guest.kernel_url.rsplit("/", maxsplit=1)[0] + "/"
    try:
        with urllib.request.urlopen(netboot, timeout=60) as response:
            index = response.read().decode(errors="replace")
    except Exception as e:
        raise Unavailable(f"unable to list {netboot}: {e}")

    names = sorted(set(re.findall(r'System\.map-[0-9][^"<]*-lts', index)))
    if not names:
        raise Unavailable(f"{netboot} serves no System.map")

    return fetch_cached(guest, names[-1], netboot + names[-1])


def host_kernel() -> Path:
    return Path("/boot") / ("vmlinuz-" + os.uname().release)


def build_initramfs(staging_dir: Path, busybox: Path, module: Path,
                    port: str | None) -> Path:
    root = staging_dir / "root"
    (root / "bin").mkdir(parents=True)
    (root / "dev").mkdir()
    (root / "proc").mkdir()

    shutil.copy(busybox, root / "bin" / "busybox")
    (root / "bin" / "busybox").chmod(0o755)
    for applet in ("sh", "insmod", "dmesg", "mount", "poweroff", "cat", "seq"):
        (root / "bin" / applet).symlink_to("busybox")

    shutil.copy(module, root / module.name)

    init = root / "init"
    init.write_text("\n".join([
        "#!/bin/sh",
        "mount -t proc proc /proc",
        "mount -t devtmpfs dev /dev",
        f"insmod /{module.name} || echo {MODULE_LOAD_FAILED_MARKER}",
        "for i in 1 2 3 4 5 6 7 8; do",
        "  (for j in $(seq 200); do cat /proc/self/maps /proc/1/status > /dev/null; done) &",
        "done",
        "wait",
        *(BRIDGE if port is not None else []),
        "dmesg",
        "exec sh",
        "",
    ]))
    init.chmod(0o755)

    initramfs = staging_dir / "initramfs.cpio"
    names = subprocess.run(["find", "."], cwd=root, check=True, capture_output=True)
    with initramfs.open("wb") as f:
        subprocess.run(["cpio", "--create", "--format=newc", "--quiet"],
                       cwd=root, input=names.stdout, stdout=f, check=True)

    return initramfs


def fetch_cached(guest: "Guest", name: str, url: str) -> Path:
    cached = cache_dir(guest) / name
    if cached.exists():
        return cached

    cached.parent.mkdir(parents=True, exist_ok=True)
    staging = cached.with_suffix(".partial")
    try:
        with urllib.request.urlopen(url, timeout=60) as response, staging.open("wb") as f:
            shutil.copyfileobj(response, f)
    except Exception as e:
        staging.unlink(missing_ok=True)
        raise Unavailable(f"unable to download {url}: {e}")
    staging.replace(cached)

    return cached


def cache_dir(guest: "Guest") -> Path:
    base = os.environ.get("XDG_CACHE_HOME")
    root = Path(base) if base is not None else Path.home() / ".cache"
    return root / "frida-tests" / f"qemu-{guest.arch}"


class Unavailable(Exception):
    pass


class Guest(NamedTuple):
    arch: str
    qemu_binary: str
    kernel_url: str
    machine: str = ""
    cpu: str = ""
    console: str = "ttyS0"
    ramdisk_url: str = ""
    live_machine: str = ""
    ecam: int = 0
    hostlink: str = "pci"
    live_cmdline: str = ""

# Paging comes up long before this, so the panic that follows the missing root filesystem is a
# safely late — and unmistakable — sign that the kernel has finished with its page tables.
BOOT_COMPLETE_MARKER = "Kernel panic"

# Alpine's netboot kernels are the smallest stock ones that are trivially fetchable. They are
# not versioned in-place, so treat whatever the mirror currently serves as the fixture; the
# tests only care that it is a Linux kernel that enables paging.
ALPINE_NETBOOT_URL = "https://dl-cdn.alpinelinux.org/alpine/v3.21/releases/{0}/netboot/vmlinuz-lts"
ALPINE_RAMDISK_URL = "https://dl-cdn.alpinelinux.org/alpine/v3.21/releases/{0}/netboot/initramfs-lts"

GUESTS = {
    guest.arch: guest
    for guest in [
        Guest("x86", "qemu-system-i386", ALPINE_NETBOOT_URL.format("x86"),
              ramdisk_url=ALPINE_RAMDISK_URL.format("x86"), live_machine="pc",
              live_cmdline="nokaslr"),
        Guest("x86_64", "qemu-system-x86_64", ALPINE_NETBOOT_URL.format("x86_64"),
              ramdisk_url=ALPINE_RAMDISK_URL.format("x86_64"), live_machine="pc",
              live_cmdline="nokaslr"),
        Guest("arm", "qemu-system-arm", ALPINE_NETBOOT_URL.format("armv7"),
              machine="virt", cpu="cortex-a7", console="ttyAMA0",
              ramdisk_url=ALPINE_RAMDISK_URL.format("armv7"), hostlink="mmio",
              # Left where it lands by default, the configuration space is above what a
              # kernel without LPAE can address.
              live_machine="virt,highmem=off", ecam=0x3f000000),
    ]
}

KMOD_GUEST_ARCH = "x86_64"

BUSYBOX_URL = "https://busybox.net/downloads/binaries/1.35.0-x86_64-linux-musl/busybox"

VIRTIO_SERIAL_ID = "frida-vserial"

GDB_ID = "frida-gdb"

# Newest model that boots 95; older ones lack the CMOVs the agent emits.
class WindowsGuest(NamedTuple):
    description: str
    emulator: str
    cpu: str
    memory: int
    boot_seconds: int
    disk_interface: str
    settle: object


WINDOWS_GUESTS = {
    # Old enough for Windows 95, new enough for the CMOV the agent is built with.
    "win95": WindowsGuest("Windows 95", "qemu-system-i386", "pentium3", 128, 120, "ide",
                          settle_win95_desktop),
    "winxp": WindowsGuest("Windows XP", "qemu-system-i386", "core2duo", 512, 600, "ide", None),
    "winxp64": WindowsGuest("Windows XP x64", "qemu-system-x86_64", "core2duo", 1024, 900, "ide",
                            None),
}

DEBUGCON_ID = "frida-debugcon"
DEBUGCON_PORT = 0xe9

SNAPSHOT_NAME = "booted"
TEXT_MODE_SIZE = (720, 400)
WIN95_BOOT_TIMEOUT_SECONDS = 900
WIN95_BOOT_POLL_SECONDS = 15
WIN95_SHELL_SETTLE_SECONDS = 90
WIN95_DIALOG_SETTLE_SECONDS = 30
PORT_DEVICE_ID = "hostlink.port"
PORT_CHARDEV_ID = "vserial0"
PORT_REMOVAL_TIMEOUT_SECONDS = 20
PORT_REMOVAL_POLL_SECONDS = 0.1
QMP_CONNECT_TIMEOUT_SECONDS = 60
QMP_CONNECT_POLL_SECONDS = 0.5

PORT_NAME = "frida"
PORT_PATH = "/dev/vport0p1"

BRIDGE = [
    "exec 3<> /dev/frida",
    f"exec 4<> {PORT_PATH}",
    "while :; do cat <&3 >&4; done &",
    "while :; do cat <&4 >&3; sleep 1; done &",
]

AGENT_READY_MARKER = "frida: listening on /dev/"
LIVE_READY_MARKER = "recovery shell launched"

LINK_ANSWER_MARKER = "frida-link "

LET_THE_LINK_GO = (
    "tag=frida; echo virtio0 > /sys/bus/virtio/drivers/virtio_console/unbind; "
    "echo $tag-link-gone"
)

LINK_GONE_MARKER = "frida-link-gone"

WHERE_THE_LINK_IS = (
    "tag=frida; echo $tag-link "
    "$(basename $(readlink -f /sys/bus/virtio/devices/virtio0/..) | cut -d. -f1) "
    "$(awk '/virtio0/{sub(/:/,\"\",$1); print $1}' /proc/interrupts)"
)
MODULE_LOAD_FAILED_MARKER = "frida-kmod-load-failed"

SLEPT_WHERE_FORBIDDEN_MARKER = "Voluntary context switch within RCU"


if __name__ == "__main__":
    try:
        main()
    except Unavailable as e:
        print(f"unavailable: {e}", file=sys.stderr)
        sys.exit(2)
