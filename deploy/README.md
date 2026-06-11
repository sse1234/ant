# Deploying ANT to real hardware

The 10-minute checklist for an Amiga session. This kit reflects the
author's setup — a Raspberry Pi serial bridge + HTTP mirror from the
companion **amiga-tools** project — but the daemon itself only needs two
files on the Amiga; deliver them any way you like (floppy, CF card, FTP).

Everything here is **untested on real hardware** until the first session —
the emulator rig has validated the daemon, not a real TCP/IP stack and NIC.

## 0. Preconditions

- Amiga booted, network up (`ping <amiga-ip>` from the modern box)
- Pi reachable over SSH, creds in `~/dev/.env.local`
  (`ANT_PI_HOST=<pi-address>` if not `raspberrypi.local`)
- Server built: `make docker` in `server/` (or grab the CI artifact)

## 1. Stage + install

```
python3 deploy/deploy.py --serial
```

Stages `ant-server` + `ant-restart` on the Pi mirror, then over the serial
bridge: fetches both via `amiget local/...` into `C:` and `S:`, sets the
exec bit. (Without `--serial` it stages only and prints the manual
commands.)

## 2. First manual test

```
python3 "$AMIGA_TOOLS"/amiga_serial.py "Run >NIL: C:ant-server"
python3 client/ant.py --host <amiga-ip> run 'Echo "hello from real iron"'
```

If that answers: celebrate, then make it permanent.

## 3. Make it survive reboots

Append `deploy/User-Startup.snippet` to `S:User-Startup` — **after** the
network bring-up block (no network, no daemon). Serial one-line `Echo`
appends have truncated User-Startup files before; small steps, verify,
keep a backup:

```
python3 "$AMIGA_TOOLS"/amiga_serial.py "Copy S:User-Startup S:User-Startup.bak"
python3 "$AMIGA_TOOLS"/amiga_serial.py "Echo >>S:User-Startup \"*NStack 16384*NRun >NIL: Execute S:ant-restart\""
python3 "$AMIGA_TOOLS"/amiga_serial.py "Type S:User-Startup"
```

Reboot, wait for the network, then `python3 client/ant.py run 'Version'`.

## 4. Soak test

Leave a long-running loop going and see if the connection survives:

```
while true; do python3 client/ant.py run 'Date' || echo "DROP $(date)"; sleep 30; done
```

Drops here point at the NIC driver / TCP stack, not ANT — exactly the
experiment that flaky telnet daemons never got.

## Rollback

The serial bridge is untouched by all of this. Worst case:
`Copy S:User-Startup.bak S:User-Startup` + reboot.
