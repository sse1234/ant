# ant — Amiga Network Terminal

![build](https://github.com/sse1234/ant/actions/workflows/build.yml/badge.svg)

A from-scratch remote shell over TCP/IP for classic AmigaOS 3.x, so a
networked Amiga (any bsdsocket-capable stack: Roadshow, AmiTCP, ...) can be
driven from a modern machine at LAN speed — no serial cable, no telnet
daemon of dubious vintage.

```
modern box ──TCP :6860──► Amiga NIC/Wi-Fi ──► bsdsocket.library
                                                    │
                                               ant-server (C, 68k)
                                                    │
                                          T:/RAM: capture (phase 1)
                                          console handler (phase 2)
                                                    │
                                              AmigaDOS shell
```

## Why from scratch

Native TelNetD setups on AmigaOS 3.x proved unreliable in practice, and no
maintained network terminal for the classic OS exists. A single-purpose
daemon kept deliberately small can be auto-restarted from `S:User-Startup`
and tuned to real use: scripted command execution first, interactive
sessions next.

Reference material worth mining (not dependencies): AmiTCP `telnetd`
source on Aminet, Roadshow SDK examples, `telser.device` (telnet-as-
serial-device — its existence proves the socket-backed-console concept on
this platform).

## Phased plan

| Phase | Deliverable | Status |
| ----- | ----------- | ------ |
| 0 | TCP echo daemon — toolchain + bsdsocket + network path | ✅ folded into 1 |
| 1 | Command executor: one command in, output + return code back | ✅ rig-verified, 40 ms/cmd |
| 2 | Interactive terminal: custom DOS console handler, raw mode | next |
| 3 | Hardening: shared-secret auth, keepalives, reconnect, multi-session | later |

Protocol (phase 1): client sends a command line; server runs it via
`System()` and answers `ANT1 rc=<n> len=<n>\n` followed by exactly `len`
raw output bytes. Many commands per connection, binary-safe.

See [docs/PLAN.md](docs/PLAN.md) for checklists and the hard-won
AmigaOS-daemon lessons (requester suppression, stack sizes, ...).

## Dev loop — no real Amiga required

FS-UAE (and Amiberry) implement UAE `bsdsocket.library` emulation that maps
Amiga socket calls onto host sockets (`bsdsocket_library = 1`). The rig in
`rig/` boots a minimal Workbench from a host directory straight into the
daemon:

```
# once: populate rig/boot/ with C:/Libs:/L: from an AmigaVision (or any
# Workbench) disk image — OS files are copyrighted and stay gitignored
./rig/setup-rig.sh

# terminal 1
fs-uae rig/ant-dev.fs-uae

# terminal 2
ANT_HOST=localhost python3 client/ant.py run 'List ANT:'
ANT_HOST=localhost python3 client/ant.py shell
```

`setup-rig.sh` needs an RDB disk image with a Workbench partition and the
[pfs3](https://github.com/metaneutrons/pfs3) tool for extraction; point it
at yours with `HDF=` / `PFS3=` / `KICK=`. Final testing belongs on real
hardware — the UAE stack is not Roadshow.

## Toolchain

bebbo's amiga-gcc via the `amigadev/crosstools:m68k-amigaos` Docker image:

```
cd server && make docker        # → ant-server (AmigaOS hunk executable)
```

On Apple Silicon, colima with `--vm-type vz --vz-rosetta` runs the amd64
image at near-native speed. CI builds the binary on every push and uploads
it as an artifact. Alternative: vbcc + NDK 3.2.

## Client

```
python3 client/ant.py run 'Version'        # one command, rc as exit code
python3 client/ant.py shell                # REPL
```

Target host: `--host`, or `ANT_HOST` in the environment (default `amiga`).

## Layout

```
ant/
├── server/           # Amiga-side daemon (C, 68k) + Makefile
├── client/ant.py     # modern-side client
├── rig/              # FS-UAE dev rig (setup script + config + boot dir)
├── deploy/           # real-hardware deployment kit + checklist
└── docs/PLAN.md      # phase checklists, open questions, lessons
```

## Status

Phase 1 verified end-to-end in the emulator rig: 40 ms average command
round-trip, output and return codes relayed byte-exact. Real-hardware
deployment kit ready (`deploy/`), awaiting an Amiga session. Phase 2
(interactive console handler) is next.

## License

MIT — see [LICENSE](LICENSE).
