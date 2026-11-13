"""Tests against a real ESP32 flashed with examples/basic. See README, "On-device tests"."""

from __future__ import annotations

import os
import subprocess
import time
from pathlib import Path

import pytest
import requests
from device import Device

REPO = Path(__file__).resolve().parent.parent


def pytest_addoption(parser: pytest.Parser) -> None:
    group = parser.getgroup("device")
    group.addoption(
        "--device",
        default=os.environ.get("PU_DEVICE"),
        help="IP address or hostname of the device under test (or set PU_DEVICE)",
    )
    group.addoption("--admin-user", default="admin")
    group.addoption(
        "--admin-password",
        default=os.environ.get("PU_ADMIN_PASSWORD"),
        help="admin password the firmware was built with, if any (or set PU_ADMIN_PASSWORD)",
    )
    group.addoption(
        "--flash",
        metavar="ENV",
        help="build this environment of examples/basic (with --admin-password compiled in, "
        "if given) and flash it over WiFi before testing",
    )
    group.addoption(
        "--ota-password",
        help="admin password of the firmware currently on the device, if it differs from "
        "--admin-password (only needed together with --flash)",
    )
    group.addoption(
        "--disruptive",
        action="store_true",
        help="also run tests that restart the device or change stored settings",
    )


def pytest_configure(config: pytest.Config) -> None:
    config.addinivalue_line(
        "markers", "disruptive: restarts the device or changes stored settings (needs --disruptive)"
    )


def pytest_collection_modifyitems(config: pytest.Config, items: list[pytest.Item]) -> None:
    if not config.getoption("--device"):
        skip = pytest.mark.skip(reason="no device given: pass --device <ip> or set PU_DEVICE")
        for item in items:
            item.add_marker(skip)
        return
    if not config.getoption("--disruptive"):
        skip = pytest.mark.skip(reason="restarts the device: pass --disruptive to run")
        for item in items:
            if "disruptive" in item.keywords:
                item.add_marker(skip)


def _flash(env: str, device: Device, current: Device) -> None:
    try:
        previous = current.boot_number()
    except (requests.RequestException, AssertionError):
        previous = None
    command = ["pio", "run", "-d", "examples/basic", "-e", env, "-t", "upload"]
    command += ["--upload-port", device.host]
    environment = dict(os.environ)
    if device.auth:
        define = f'-DPU_EXAMPLE_ADMIN_PASSWORD=\\"{device.auth[1]}\\"'
        environment["PLATFORMIO_BUILD_FLAGS"] = define
    if current.auth:
        environment["PLATFORMIO_UPLOAD_FLAGS"] = f"--auth={current.auth[1]}"
    subprocess.run(command, cwd=REPO, env=environment, check=True)
    device.wait_for_boot(previous)


@pytest.fixture(scope="session")
def device(pytestconfig: pytest.Config) -> Device:
    host = pytestconfig.getoption("--device")
    user = pytestconfig.getoption("--admin-user")
    password = pytestconfig.getoption("--admin-password")
    dev = Device(host, user, password)
    try:
        dev.get("/api/pu/status")
    except requests.RequestException as error:
        pytest.exit(f"device {dev.host} is not reachable: {error}", returncode=2)
    env = pytestconfig.getoption("--flash")
    if env:
        _flash(env, dev, Device(host, user, pytestconfig.getoption("--ota-password") or password))
        time.sleep(1)
    return dev


@pytest.fixture
def console(device: Device):
    with device.web_console() as web_console:
        web_console.drain(quiet=0.3)  # skip anything still queued from earlier activity
        yield web_console
