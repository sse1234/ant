# ANT plan

## Phase 0 — smoke test

- [ ] Pick + install toolchain (Docker amigadev/crosstools vs vbcc)
- [ ] Compile `server/main.c` echo daemon
- [ ] FS-UAE rig: config with `bsdsocket_library = 1`, shared dir to get the
      binary in (or use AmigaVision DH0 + a mounted host directory)
- [ ] `nc localhost 6860` echoes
- [ ] Same on real hardware (deploy via stage_binary.py, run from AUX shell)

## Phase 1 — command executor

- [ ] Protocol: newline-delimited command in, output + exit sentinel back
      (keep it dumb; no telnet negotiation)
- [ ] Per-connection: `System("...", SYS_Input/SYS_Output → PIPE: pair)`
- [ ] Pump socket ⇄ pipe with WaitSelect + dos signals
- [ ] `client/ant.py run "<cmd>"` — drop-in replacement for amiga_serial.py
      scripted use
- [ ] Auto-start from S:User-Startup + auto-restart wrapper
- [ ] Measure: command round-trip vs 9600-baud serial

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
