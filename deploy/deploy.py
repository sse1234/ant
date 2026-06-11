#!/usr/bin/env python3
"""Deploy ant-server to the real Amiga via the Pi staging path.

    python3 deploy/deploy.py             # stage on the Pi, print checklist
    python3 deploy/deploy.py --serial    # also drive the install over the
                                         # Pi serial bridge (amiga-tools)

Stages server/ant-server and deploy/ant-restart to the Pi proxy's static
dir (verbatim bytes, like amiga-tools stage_binary.py), so the Amiga can
fetch them with `amiget local/<name>`. With --serial, runs the fetch +
install commands through amiga-tools/amiga_serial.py.

Needs $amiga_user/$amiga_pw in ~/dev/.env.local. The User-Startup append
stays manual on purpose — see deploy/README.md (serial Echo append has
truncated S:User-Startup before; small steps + verify).

NOTE: untested against real hardware until the next Amiga session.
"""
from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path

import paramiko

ENV = Path.home() / "dev" / ".env.local"
PI_HOST = "192.168.178.140"
REMOTE_DIR = "/opt/amiga-proxy/static"
HERE = Path(__file__).resolve().parent
AMIGA_TOOLS = Path.home() / "dev" / "amiga-tools"

# (serial command, extra amiga_serial.py args) — network fetches get the
# long-lull treatment like the amiga-tools README examples.
SLOW = ["--timeout", "60", "--quiet", "2", "--lull", "15"]
SERIAL_STEPS: list[tuple[str, list[str]]] = [
    ("CD C:", []),
    ("C:amiget local/ant-server", SLOW),
    ("Protect C:ant-server +e", []),
    ("CD S:", []),
    ("C:amiget local/ant-restart", SLOW),  # absolute C:amiget — CWD=S: gotcha
    ("CD SYS:", []),
]


def env() -> dict[str, str]:
    out = {}
    for ln in ENV.read_text(encoding="utf-8").splitlines():
        m = re.match(r'\$(\w+)\s*=\s*"([^"]*)"', ln.strip())
        if m:
            out[m.group(1)] = m.group(2)
    return out


def stage() -> None:
    files = {
        "ant-server": HERE.parent / "server" / "ant-server",
        "ant-restart": HERE / "ant-restart",
    }
    for src in files.values():
        if not src.is_file():
            sys.exit(f"missing {src} — build the server first (make in server/)")
    e = env()
    c = paramiko.SSHClient()
    c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    c.connect(PI_HOST, username=e["amiga_user"], password=e["amiga_pw"], timeout=15)
    try:
        sftp = c.open_sftp()
        try:
            for name, src in files.items():
                data = src.read_bytes()
                dst = f"{REMOTE_DIR}/{name}"
                with sftp.open(dst, "wb") as f:
                    f.write(data)
                sftp.chmod(dst, 0o644)
                print(f"staged {name} ({len(data):,} bytes) -> pi:{dst}")
        finally:
            sftp.close()
    finally:
        c.close()


def serial_install() -> None:
    runner = AMIGA_TOOLS / "amiga_serial.py"
    if not runner.is_file():
        sys.exit(f"missing {runner} — clone amiga-tools next to ant/")
    for cmd, extra in SERIAL_STEPS:
        print(f"serial> {cmd}")
        subprocess.run(
            [sys.executable, str(runner), *extra, cmd],
            cwd=AMIGA_TOOLS, check=True,
        )


def main() -> int:
    stage()
    if "--serial" in sys.argv[1:]:
        serial_install()
        print("\ninstalled. Test:  python3 client/ant.py run 'Echo hello'")
        print("then do the User-Startup append — see deploy/README.md")
    else:
        print("\nstaged only. Manual steps on/for the Amiga:")
        for cmd, _ in SERIAL_STEPS:
            print(f"  python3 {AMIGA_TOOLS}/amiga_serial.py \"{cmd}\"")
        print("then:  python3 client/ant.py run 'Echo hello'")
        print("and the User-Startup append — see deploy/README.md")
    return 0


if __name__ == "__main__":
    sys.exit(main())
