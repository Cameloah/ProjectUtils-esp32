"""Helpers for talking to a device that runs the example firmware."""

from __future__ import annotations

import re
import time
from collections.abc import Iterator
from contextlib import contextmanager

import requests
from requests.auth import HTTPDigestAuth
from websockets.sync.client import ClientConnection, connect

_BOOT_NUMBER = re.compile(r"Boot number (\d+)")


class Device:
    """HTTP access to the device, with the admin login (digest) applied if a password is set."""

    def __init__(self, host: str, user: str, password: str | None) -> None:
        self.host = host
        self.base = f"http://{host}"
        self.auth = (user, password) if password else None

    def _login(self, auth: bool):
        return HTTPDigestAuth(*self.auth) if auth and self.auth else None

    def get(self, path: str, *, auth: bool = True, **kwargs) -> requests.Response:
        kwargs.setdefault("timeout", 5)
        kwargs.setdefault("auth", self._login(auth))
        return requests.get(self.base + path, **kwargs)

    def post(
        self, path: str, data: dict | None = None, *, auth: bool = True, **kwargs
    ) -> requests.Response:
        kwargs.setdefault("timeout", 5)
        kwargs.setdefault("auth", self._login(auth))
        return requests.post(self.base + path, data=data or {}, **kwargs)

    def json(self, path: str) -> dict:
        response = self.get(path)
        response.raise_for_status()
        return response.json()

    def status(self) -> dict:
        return self.json("/api/pu/status")

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
    def web_console(self, *, token: str | None = None, auth: bool = True) -> Iterator[WebConsole]:
        """Opens the console websocket with a token, fetched with the admin login if not given."""
        if token is None and self.auth and auth:
            token = self.json("/api/pu/console/token")["token"]
        url = f"ws://{self.host}/api/pu/console/ws" + (f"?token={token}" if token else "")
        with connect(url) as socket:
            yield WebConsole(socket)


class HomePage:
    """The example's home page, addressed by widget label the way a user would."""

    def __init__(self, device: Device, path: str = "/") -> None:
        self.device = device
        self.path = path
        self.layout = device.json(f"/api/pu/page?path={path}")
        self.labels = [widget["label"] for widget in self.layout["widgets"]]

    def index(self, label: str, kind: str | None = None) -> int:
        for index, widget in enumerate(self.layout["widgets"]):
            if widget["label"] == label and kind in (None, widget["kind"]):
                return index
        raise AssertionError(f"no widget {label!r} on the page")

    def value(self, label: str, kind: str | None = None):
        values = self.device.json(f"/api/pu/page/values?path={self.path}")["values"]
        return values[self.index(label, kind)]

    def act(self, label: str, action: str, value=None, kind: str | None = None):
        data = {"path": self.path, "widget": self.index(label, kind), "action": action}
        if value is not None:
            data["value"] = value
        return self.device.post("/api/pu/page/widget", data)

    def wait_for(self, label: str, expected, timeout: float = 3, kind: str | None = None):
        """Waits until the widget shows `expected`; returns the last value seen."""
        deadline = time.monotonic() + timeout
        while True:
            current = self.value(label, kind)
            if current == expected or time.monotonic() > deadline:
                return current
            time.sleep(0.1)


class WebConsole:
    """The web console's websocket: text messages, one or more lines each."""

    def __init__(self, socket: ClientConnection) -> None:
        self._socket = socket

    def send(self, text: str) -> None:
        self._socket.send(text)

    def lines(self, count: int, timeout: float = 3) -> list[str]:
        """Collects output lines until `count` have arrived or `timeout` seconds have passed."""
        lines: list[str] = []
        deadline = time.monotonic() + timeout
        while len(lines) < count:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                break
            try:
                message = self._socket.recv(timeout=remaining)
            except TimeoutError:
                break
            if isinstance(message, str):
                lines.extend(message.split("\n"))
        return lines

    def drain(self, quiet: float = 0.5) -> list[str]:
        """Collects lines until nothing has arrived for `quiet` seconds."""
        lines: list[str] = []
        while True:
            batch = self.lines(1, timeout=quiet)
            if not batch:
                return lines
            lines.extend(batch)
