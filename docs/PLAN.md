# ANT plan

## Phase 0 — smoke test

- [x] Pick + install toolchain — colima (vz + Rosetta) +
      amigadev/crosstools:m68k-amigaos Docker image
- [x] Compile `server/main.c` echo daemon — clean build, AmigaOS hunk
      executable, 13.7 KB
- [x] FS-UAE rig: `rig/ant-dev.fs-uae` — bare-Kickstart boot from host dir
      `rig/boot/`, `bsdsocket_library = 1` (FS-UAE 3.2.35 via brew cask)
- [x] Port answers from the Mac side
      (phase 0 was folded into phase 1 verification)

## Phase 1 — command executor

- [x] Protocol: command line in → `ANT1 rc=<n> len=<n>` header + raw output
      bytes back; many commands per connection; binary-safe
- [x] Execution: synchronous `System()` (shell internals work), stdin NIL:,
      output captured to T: (RAM: fallback) and streamed back.
      PIPE:-streaming during execution deferred — phase 2 solves it properly
- [x] `client/ant.py run "<cmd>"` + `shell` REPL; rc mirrored as exit code
- [x] Verified e2e in FS-UAE rig: Echo round-trip, error text + rc=10
      propagation for unknown commands
- [x] Measured: **40 ms avg round-trip** (20-command burst over one
      connection, emulated 68020) vs seconds-per-command on 9600-baud serial
- [ ] Deploy to real Amiga — **kit ready** (`deploy/deploy.py --serial` +
      `deploy/README.md` checklist), execution needs the hardware
- [ ] Auto-start from S:User-Startup + auto-restart wrapper — snippet +
      S:ant-restart script prepared in deploy/, append is step 3 of the
      checklist
- [ ] Soak test over Wi-Fi (the TelNetD-unreliability question) — loop
      one-liner in deploy/README.md step 4
- [x] CI: GitHub Actions builds ant-server in the crosstools container on
      every push, checks hunk magic, uploads the binary as artifact

### Rig niceties

- [x] `rig/setup-rig.sh` extracts C:/Libs:/L: from the AmigaVision HDF
      (pfs3) so the rig behaves like a real Workbench — List/Avail/Type/
      Copy/Execute etc. all verified through ANT. OS files stay gitignored.
      Note: `pfs3 extract -o DIR /Drawer` writes the drawer *contents* into
      DIR — give each drawer its own target.

### Hard-won rig/daemon lessons (already encoded in code/rig)

- Bare-Kickstart boots have only ROM-internal shell commands (Echo, CD,
  Stack...) — no MakeDir/Assign/List. Keep the rig startup-sequence to
  internals only.
- Boot shell default stack is 4k; `Stack 16384` before launching the daemon.
- `pr_WindowPtr = -1` in the daemon or any missing-volume Open() raises a
  blocking "Please insert volume" requester. This bit us in the rig
  (`Open("T:...")` with no T: assign) and matches a known real-hardware
  failure mode (requesters silently freezing remote shells).

## Phase 2 — interactive terminal

- [ ] Custom DOS handler (ANT-CON:) implementing console packets
      (ACTION_READ/WRITE/WAIT_CHAR/DISK_INFO...) backed by the socket
- [ ] `NewShell ANT-CON:` per connection
- [ ] Raw mode pass-through for full-screen programs
- [ ] Line discipline / local echo decisions
- [ ] `client/ant.py` interactive mode (tty raw, like ssh)

## Phase 3 — hardening

- [ ] Shared-secret auth (key from ~/dev/.env.local; LAN-only is the threat
      model)
- [ ] TCP keepalive + client auto-reconnect
- [ ] Multiple concurrent sessions
- [ ] Graceful behavior when Wi-Fi drops mid-session

## Open questions

- Root cause of TelNetD unreliability — Roadshow inetd? Wi-Fi driver under
  sustained TCP? If it's the driver, ANT inherits the problem; mitigations
  are keepalive/reconnect, not magic. Worth a structured soak test early.
- bsdsocket sockets are per-task: passing an accepted socket to a child
  process needs ReleaseSocket/ObtainSocket. Decide single-task vs
  task-per-connection in phase 1.
- Port: 6860 (68060 homage). Revisit if anything collides.
- Stack sizes — 68k child processes with default 4k stacks are a classic
  source of "unreliable"; be generous and explicit everywhere.
