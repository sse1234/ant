# ant — Amiga Network Terminal

![build](https://github.com/sse1234/ant/actions/workflows/build.yml/badge.svg)

A from-scratch remote shell over TCP/IP for classic AmigaOS (3.x, Roadshow),
so the Warp 560's working Wi-Fi can replace the Pi serial bridge as the
day-to-day control path.

```
Mac ──TCP :6860──► Warp 560 Wi-Fi ──► Roadshow bsdsocket.library
                                            │
                                       ant-server (C, 68k)
                                            │
                                  PIPE: / custom handler
                                            │
                                      AmigaDOS shell
```

The serial bridge (see `~/dev/amiga-tools`) stays as fallback and as the
deployment path for the server binary itself.

## Why from scratch

Native TelNetD under Roadshow proved unreliable in practice, and no other
maintained network terminal for AmigaOS 3.x exists. A single-purpose daemon
we control end-to-end can be kept deliberately small, auto-restarted from
`S:User-Startup`, and tuned to our actual use (scripted command execution
from `amiga_serial.py`-style tooling, later interactive use).

Reference material worth mining (not dependencies): AmiTCP `telnetd` source
on Aminet, Roadshow SDK examples, `telser.device` (telnet-as-serial-device —
its existence proves the socket-backed-console concept on this platform).

## Phased plan

| Phase | Deliverable | Difficulty |
| ----- | ----------- | ---------- |
| 0 | TCP echo daemon — proves toolchain + bsdsocket + emulator/hw network path | trivial |
| 1 | Command executor: per-connection `System()` with `PIPE:` redirected stdin/stdout, Python client. Replaces 9600-baud serial for scripted use | moderate |
| 2 | Interactive terminal: custom DOS console handler (socket-backed `AUX:` equivalent), raw mode, window size | the real work |
| 3 | Hardening: shared-secret auth, keepalives, reconnect, multi-session | incremental |

Phase 1 already delivers the main win (LAN-speed command round-trips vs.
9600 baud). Phase 2 is where DOS-packet arcana lives; it's well documented
(DOS manual, Guru Book) but fiddly.

## Dev loop

**No real Amiga needed for iteration.** FS-UAE (and Amiberry) implement UAE
`bsdsocket.library` emulation that maps Amiga socket calls onto host
sockets:

```
# in the .fs-uae config:
bsdsocket_library = 1
```

So: cross-compile on macOS → run in FS-UAE (AmigaVision DH0 boots a normal
Workbench) → connect from a Mac terminal to localhost. Deploy to the real
Amiga afterwards via `stage_binary.py` + `amiget` from amiga-tools.

Caveat: UAE bsdsocket emulation has quirks (WaitSelect/signal semantics),
and the real Roadshow stack is the actual target — final testing happens on
hardware.

## Toolchain

bebbo amiga-gcc via the `amigadev/crosstools:m68k-amigaos` Docker image
(amd64), run through **colima** (`--vm-type vz --vz-rosetta` so x86
containers go through Rosetta 2 at near-native speed). Setup that was done
on this machine:

```
brew install colima docker
colima start --vm-type vz --vz-rosetta --cpu 4 --memory 4
cd server && make docker        # → ant-server (AmigaOS hunk executable)
```

After reboot: `colima start` brings the VM back. Alternative toolchain if
Docker ever annoys: vbcc native macOS + NDK 3.2.

## Layout

```
ant/
├── README.md
├── docs/PLAN.md          # phase checklists, open questions
├── server/               # Amiga-side daemon (C, 68k)
│   ├── main.c            # phase 0: echo daemon skeleton (untested until toolchain lands)
│   └── Makefile
└── client/
    └── ant.py            # Mac-side client
```

## Status

**Phase 1 working end-to-end in the FS-UAE rig**: 40 ms avg command
round-trip (vs seconds over 9600-baud serial), output + return code
faithfully relayed, binary-safe protocol.

```
# once: populate rig/boot/ with C:/Libs:/L: from the AmigaVision HDF
# (copyrighted OS files — gitignored, never committed)
./rig/setup-rig.sh

# terminal 1 — boots straight into the daemon
/Applications/FS-UAE.app/Contents/MacOS/fs-uae rig/ant-dev.fs-uae

# terminal 2
ANT_HOST=localhost python3 client/ant.py run 'List ANT:'
ANT_HOST=localhost python3 client/ant.py shell
```

Next: deploy to the real Amiga (Roadshow + Warp 560), S:User-Startup
autostart, Wi-Fi soak test. Then phase 2 (interactive console handler).
