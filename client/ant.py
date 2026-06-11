#!/usr/bin/env python3
"""ant — client for the Amiga Network Terminal daemon.

Phase 0: line client for the echo server. Later grows `run` (scripted
command execution, replacing amiga_serial.py) and `shell` (interactive).

usage:  python3 ant.py [host] [port]          # interactive echo test
"""
from __future__ import annotations

import socket
import sys

DEFAULT_HOST = "192.168.178.139"  # Amiga; use localhost against FS-UAE
DEFAULT_PORT = 6860


def main() -> int:
    host = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_HOST
    port = int(sys.argv[2]) if len(sys.argv) > 2 else DEFAULT_PORT

    with socket.create_connection((host, port), timeout=10) as s:
        s.settimeout(5)
        print(s.recv(1024).decode("latin-1"), end="")
        try:
            for line in sys.stdin:
                s.sendall(line.encode("latin-1"))
                if line.strip() == "quit":
                    break
                print(s.recv(4096).decode("latin-1"), end="")
        except KeyboardInterrupt:
            pass
    return 0


if __name__ == "__main__":
    sys.exit(main())
