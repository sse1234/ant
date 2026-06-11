# Deploying ANT to the real Amiga

The 10-minute checklist for the next hardware session. Everything here is
**untested on real hardware** until that session happens — the rig has
validated the daemon, not the Roadshow/Warp-560 path.

## 0. Preconditions

- Amiga booted, Wi-Fi up (`ping 192.168.178.139` from the Mac)
- Pi reachable (`ssh` to `192.168.178.140` works / `~/dev/.env.local` set)
- Server built: `make docker` in `server/` (or grab the CI artifact)

## 1. Stage + install

```
python3 deploy/deploy.py --serial
```

This stages `ant-server` + `ant-restart` on the Pi mirror, then over the
serial bridge: fetches both via `amiget local/...` into `C:` and `S:`,
sets the exec bit. (Without `--serial` it stages only and prints the
manual commands.)

## 2. First manual test

```
python3 ../amiga-tools/amiga_serial.py "Run >NIL: C:ant-server"
python3 client/ant.py run 'Echo "hello from real iron"'
```

If that answers: celebrate, then make it permanent.

## 3. Make it survive reboots

Append `deploy/User-Startup.snippet` to `S:User-Startup` — **after** the
Warp Wi-Fi + Roadshow block (no network, no daemon). Given the serial-Echo
truncation history (see amiga-tools PROJECT_MEMORY), do it in small steps
and verify:

```
python3 ../amiga-tools/amiga_serial.py "Copy S:User-Startup S:User-Startup.bak"
python3 ../amiga-tools/amiga_serial.py "Echo >>S:User-Startup \"*NStack 16384*NRun >NIL: Execute S:ant-restart\""
python3 ../amiga-tools/amiga_serial.py "Type S:User-Startup"
```

Reboot, wait for Wi-Fi, then `python3 client/ant.py run 'Version'`.

## 4. Soak test (the TelNetD question)

Leave a long-running loop going and see if the connection survives:

```
while true; do python3 client/ant.py run 'Date' || echo "DROP $(date)"; sleep 30; done
```

Drops here point at the Warp driver / Roadshow, not ANT — that's the
experiment the old TelNetD flakiness never got.

## Rollback

Serial bridge is untouched by all of this. Worst case:
`Copy S:User-Startup.bak S:User-Startup` + reboot.
