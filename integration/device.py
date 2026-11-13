"""Helpers for talking to a device that runs the example firmware."""

from __future__ import annotations

import base64
import re
import struct
import time
from collections.abc import Iterator
from contextlib import contextmanager

import requests
from websockets.sync.client import ClientConnection, connect

# WebSerial wire format: magic, packet type, then type-specific fields.
_MAGIC = b"\xab\xcd"
_TYPE_MESSAGE = 0x02
_ROW_HEADER_SIZE = 12  # magic, type and reserved bytes in front of the 2-byte row length

_BOOT_NUMBER = re.compile(r"Boot number (\d+)")


class Device:
    """HTTP access to the device, with the admin login applied if one is configured."""

    def __init__(self, host: str, user: str, password: str | None) -> None:
        self.host = host
        self.base = f"http://{host}"
        self.auth = (user, password) if password else None

    def get(self, path: str, *, auth: bool = True, **kwargs) -> requests.Response:
        kwargs.setdefault("timeout", 5)
        return requests.get(self.base + path, auth=self.auth if auth else None, **kwargs)

    def post(self, path: str, data: dict | None = None, *, auth: bool = True) -> requests.Response:
        return requests.post(
            self.base + path, data=data or {}, auth=self.auth if auth else None, timeout=5
        )

    def status(self) -> dict:
        response = self.get("/api/pu/status")
        response.raise_for_status()
        return response.json()

    def log_lines(self) -> list[str]:
        response = self.get("/api/pu/log")
        response.raise_for_status()
        return response.text.splitlines()

    def boot_number(self) -> int:
        """Boot counter the example firmware writes to the log on every start."""
        matches = _BOOT_NUMBER.findall("\n".join(self.log_lines()))
        if not matches:
            raise AssertionError("no 'Boot number' line in the device log")
        return int(matches[-1])

    def wait_for_boot(self, previous: int | None, timeout: float = 45) -> int:
        """Waits until the device has restarted and is reachable again."""
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            try:
                current = self.boot_number()
            except (requests.RequestException, AssertionError):
                current = previous
            if current != previous:
                return current
            time.sleep(0.5)
        raise AssertionError(f"device did not come back within {timeout:.0f} s after a restart")

    @contextmanager
    def web_console(self) -> Iterator[WebConsole]:
        headers = {}
        if self.auth:
            token = base64.b64encode(":".join(self.auth).encode()).decode()
            headers["Authorization"] = f"Basic {token}"
        with connect(f"ws://{self.host}/wserial", additional_headers=headers) as socket:
            yield WebConsole(socket)


class WebConsole:
    """The web console's websocket, speaking the same protocol as the browser page."""

    def __init__(self, socket: ClientConnection) -> None:
        self._socket = socket

    def send(self, text: str | bytes) -> None:
        data = text.encode() if isinstance(text, str) else text
        self._socket.send(_MAGIC + bytes([_TYPE_MESSAGE]) + struct.pack("<H", len(data)) + data)

    def rows(self, count: int, timeout: float = 3) -> list[str]:
        """Collects output rows until `count` have arrived or `timeout` seconds have passed."""
        rows: list[str] = []
        deadline = time.monotonic() + timeout
        while len(rows) < count:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                break
            try:
                frame = self._socket.recv(timeout=remaining)
            except TimeoutError:
                break
            if isinstance(frame, bytes):
                rows.extend(_parse_rows(frame))
        return rows

    def drain(self, quiet: float = 0.5) -> list[str]:
        """Collects rows until nothing has arrived for `quiet` seconds."""
        rows: list[str] = []
        while True:
            batch = self.rows(1, timeout=quiet)
            if not batch:
                return rows
            rows.extend(batch)


def _parse_rows(frame: bytes) -> list[str]:
    rows = []
    offset = 0
    header = _ROW_HEADER_SIZE + 2
    while offset + header <= len(frame) and frame[offset : offset + 2] == _MAGIC:
        (length,) = struct.unpack_from("<H", frame, offset + _ROW_HEADER_SIZE)
        start = offset + header
        rows.append(frame[start : start + length].decode(errors="replace"))
        offset = start + length
    return rows
