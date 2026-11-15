"""The admin password: set on the device, digest login only, USB-only recovery.

Sets a password through the settings page for these tests and removes it again afterwards
(two restarts), unless the device already has one (--admin-password). The recovery test needs
the USB port (--serial).
"""

from __future__ import annotations

import time

import pytest
import requests
from device import Device
from requests.auth import HTTPBasicAuth
from test_http_api import MUTATING, PROTECTED_GET
from websockets.exceptions import InvalidStatus

pytestmark = pytest.mark.disruptive

TEST_PASSWORD = "pytest-secret"


def _set_password(device: Device, password: str | None) -> Device:
    """Sets or removes the password; returns the device with the login it needs afterwards."""
    boot = device.boot_number()
    fields = {"adminProtect": "true" if password else "false", "adminPass": password or ""}
    assert device.post("/api/pu/settings", fields).status_code == 200
    after = Device(device.host, device.auth[0] if device.auth else "admin", password)
    after.wait_for_boot(boot)
    return after


@pytest.fixture(scope="module")
def protected(device: Device):
    if device.auth:
        yield device  # the device has a password already; leave it as it is
        return
    login = _set_password(device, TEST_PASSWORD)
    try:
        yield login
    finally:
        _set_password(login, None)  # works whether or not the password is still set
    assert device.status()["admin"]["set"] is False


@pytest.mark.parametrize("path", PROTECTED_GET)
def test_pages_need_the_login(protected, path):
    assert protected.get(path, auth=False).status_code == 401
    assert protected.get(path).status_code == 200


@pytest.mark.parametrize("path", MUTATING)
def test_actions_need_the_login(protected, path):
    assert protected.post(path, auth=False).status_code == 401


def test_only_digest_login_is_accepted(protected):
    response = protected.get("/api/pu/status", auth=False)
    assert response.headers["WWW-Authenticate"].startswith("Digest")
    basic = HTTPBasicAuth(*protected.auth)  # would carry the password readable
    assert requests.get(protected.base + "/api/pu/status", auth=basic, timeout=5).status_code == 401
    wrong = Device(protected.host, protected.auth[0], "wrong-password")
    assert wrong.get("/api/pu/status").status_code == 401


def test_password_never_leaves_the_device(protected):
    status = protected.status()
    assert status["admin"]["set"] is True
    assert protected.auth[1] not in protected.get("/api/pu/status").text
    assert protected.auth[1] not in protected.get("/api/pu/memory").text


def test_console_needs_a_token(protected):
    for token in (None, "0000000000000000"):
        with pytest.raises(InvalidStatus), protected.web_console(token=token, auth=False):
            pass


def test_recovery_command_does_nothing_from_the_web_console(protected):
    with protected.web_console() as console:
        console.drain(quiet=0.3)
        console.send("password --clear")
        console.drain(quiet=1.0)
    assert protected.get("/api/pu/status", auth=False).status_code == 401


def test_recovery_command_over_usb_removes_the_password(protected, pytestconfig):
    port = pytestconfig.getoption("--serial")
    if not port:
        pytest.skip("needs the USB port: pass --serial")
    serial = pytest.importorskip("serial")
    usb = serial.Serial()
    usb.port = port
    usb.baudrate = 115200
    usb.dtr = False
    usb.rts = False
    usb.timeout = 0.2
    # Opening the port can restart an ESP32-S3 (native USB); the line waits in the USB buffer
    # and is read once the firmware runs, so the device may take a while to answer again.
    with usb:
        usb.write(b"password --clear\r\n")
        usb.flush()
        deadline = time.monotonic() + 20
        while time.monotonic() < deadline:
            try:
                if protected.get("/api/pu/status", auth=False).status_code == 200:
                    break
            except requests.RequestException:
                pass  # restarting
            time.sleep(0.5)
    assert protected.get("/api/pu/status", auth=False).status_code == 200
    assert any("Admin password removed via USB" in line for line in protected.log_lines())
