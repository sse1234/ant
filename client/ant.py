#!/usr/bin/env python3
"""ant — client for the Amiga Network Terminal daemon (phase 1).

usage:
    python3 ant.py run "Echo Hello"            # one command, prints output
    python3 ant.py shell                        # REPL over one connection
    ANT_HOST=localhost python3 ant.py run ...   # e.g. against the FS-UAE rig

Protocol: send a command line, get back "ANT1 rc=<n> len=<n>\\n" + len raw
output bytes. Exit code mirrors the Amiga return code (clamped to 0-255).
"""
from __future__ import annotations

import argparse
import os
import socket
import sys

DEFAULT_HOST = os.environ.get("ANT_HOST", "192.168.178.139")
DEFAULT_PORT = int(os.environ.get("ANT_PORT", "6860"))


class AntClient:
    def __init__(self, host: str, port: int, timeout: float = 30.0):
        self.sock = socket.create_connection((host, port), timeout=10)
        self.sock.settimeout(timeout)
        self._buf = b""
        banner = self._read_line()
        if not banner.startswith(b"ANT/1"):
            raise RuntimeError(f"unexpected banner: {banner!r}")

    def _read_line(self) -> bytes:
        while b"\n" not in self._buf:
            chunk = self.sock.recv(4096)
            if not chunk:
                raise ConnectionError("server closed connection")
            self._buf += chunk
        line, self._buf = self._buf.split(b"\n", 1)
        return line

    def _read_exact(self, n: int) -> bytes:
        while len(self._buf) < n:
            chunk = self.sock.recv(65536)
            if not chunk:
                raise ConnectionError("server closed mid-response")
            self._buf += chunk
        data, self._buf = self._buf[:n], self._buf[n:]
        return data

    def run(self, cmd: str) -> tuple[int, bytes]:
        """Run one command, return (rc, output)."""
        self.sock.sendall(cmd.encode("latin-1") + b"\n")
        hdr = self._read_line().decode("ascii", "replace")
        parts = dict(p.split("=", 1) for p in hdr.split()[1:])
        rc, length = int(parts["rc"]), int(parts["len"])
        return rc, self._read_exact(length)

    def close(self) -> None:
        self.sock.close()


def main() -> int:
    ap = argparse.ArgumentParser(description="Amiga Network Terminal client")
    ap.add_argument("--host", default=DEFAULT_HOST)
    ap.add_argument("--port", type=int, default=DEFAULT_PORT)
    ap.add_argument("--timeout", type=float, default=30.0,
                    help="per-command response timeout, seconds")
    sub = ap.add_subparsers(dest="mode", required=True)
    p_run = sub.add_parser("run", help="run one command")
    p_run.add_argument("command")
    sub.add_parser("shell", help="interactive command loop")
    args = ap.parse_args()

    c = AntClient(args.host, args.port, args.timeout)
    try:
        if args.mode == "run":
            rc, out = c.run(args.command)
            sys.stdout.buffer.write(out)
            sys.stdout.buffer.flush()
            return min(max(rc, 0), 255)

        # shell mode
        print(f"ant: connected to {args.host}:{args.port} (Ctrl-D to exit)")
        while True:
            try:
                line = input("ant> ")
            except (EOFError, KeyboardInterrupt):
                print()
                return 0
            if not line.strip():
                continue
            rc, out = c.run(line)
            sys.stdout.buffer.write(out)
            sys.stdout.buffer.flush()
            if rc != 0:
                print(f"[rc={rc}]")
    finally:
        c.close()


if __name__ == "__main__":
    sys.exit(main())
