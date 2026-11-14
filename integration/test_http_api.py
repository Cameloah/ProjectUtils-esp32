"""Settings page and JSON API."""

from __future__ import annotations

import re
import time
from concurrent.futures import ThreadPoolExecutor

import pytest

LOG_LINE = re.compile(r"^(\d{4}-\d\d-\d\d \d\d:\d\d:\d\d\.\d{3}|\S+) (INFO|WARNING|ERROR): .+")
MUTATING = [
    "/api/pu/settings",
    "/api/pu/wifi/scan",
    "/api/pu/page/widget",
    "/api/pu/update/upload",
    "/api/pu/autoupdates",
    "/api/pu/update/check",
    "/api/pu/update/install",
    "/api/pu/restart",
    "/api/pu/memory/set",
    "/api/pu/memory/reset",
    "/api/pu/memory/add",
]
PROTECTED_GET = [
    "/",
    "/about",
    "/settings",
    "/update",
    "/console",
    "/memory",
    "/info",
    "/api/pu/memory",
    "/api/pu/nav",
    "/api/pu/status",
    "/api/pu/info",
    "/api/pu/log",
    "/api/pu/wifi/scan",
    "/api/pu/console/token",
    "/api/pu/page?path=/",
    "/api/pu/page/values?path=/",
]


def test_status_reports_a_connected_device(device):
    status = device.status()
    assert status["deviceName"]
    assert re.fullmatch(r"[a-z0-9-]+", status["hostname"])
    assert re.fullmatch(r"v\d+\.\d+\.\d+", status["firmware"])
    assert status["wifi"]["connected"] is True
    assert status["wifi"]["portal"] is False
    assert status["wifi"]["ssid"]
    assert status["update"]["state"] in {
        "idle", "checking", "upToDate", "available", "installing", "installed", "failed",
    }  # fmt: skip


def test_status_never_contains_the_wifi_password(device):
    wifi = device.status()["wifi"]
    assert "password" not in wifi
    assert "pass" not in wifi
    assert isinstance(wifi["passwordSet"], bool)


def test_project_route_registered_by_the_example(device):
    response = device.get("/hello")
    assert response.status_code == 200
    assert response.text == "Hello from " + device.status()["deviceName"]


def test_unknown_url_is_404_when_not_in_setup_mode(device):
    response = device.get("/definitely-not-here", allow_redirects=False)
    assert response.status_code == 404


def test_log_has_boot_messages_with_timestamps(device):
    lines = device.log_lines()
    assert lines, "log is empty"
    for line in lines:
        assert LOG_LINE.match(line), f"unexpected log line format: {line!r}"
    text = "\n".join(lines)
    assert "WiFi connected" in text
    assert "Boot number" in text


def test_log_timestamps_are_wall_clock_time_after_ntp_sync(device):
    # The example needs a few seconds after boot to get the time; the suite runs later than that.
    first = device.log_lines()[0]
    assert re.match(r"20\d\d-\d\d-\d\d ", first), f"no date in log line: {first!r}"


@pytest.mark.parametrize("path", [path for path in MUTATING if path != "/api/pu/wifi/scan"])
def test_state_changing_endpoints_refuse_get(device, path):
    assert device.get(path).status_code in {404, 405}


def _comparable(status: dict) -> dict:
    status = dict(status)
    status["address"] = {**status["address"], "current": None, "trialSeconds": None}
    return status


def _fixed(ip="192.168.1.20", subnet="255.255.255.0", gateway="192.168.1.1", **more) -> dict:
    return {"staticIP": "true", "ip": ip, "subnet": subnet, "gateway": gateway, **more}


@pytest.mark.parametrize(
    "fields",
    [
        _fixed(ip="999.1.1"),
        _fixed(subnet="255.0.255.0"),
        _fixed(gateway="10.0.0.1"),
        _fixed(ip="192.168.1.255"),
        _fixed(dns1="dns.example"),
        {"apMode": "sometimes", "apPassMode": "default"},
        {"apMode": "always", "apTimeout": "-5", "apPassMode": "default"},
        {"apMode": "always", "apGrace": "0", "apPassMode": "default"},
        {"apMode": "always", "apPassMode": "custom", "apPass": "short"},
        {"apMode": "always", "apPassMode": "secret"},
    ],
    ids=["bad-ip", "bad-mask", "gateway-elsewhere", "broadcast", "bad-dns", "bad-mode",
         "negative-timeout", "zero-grace", "short-password", "bad-password-choice"],
)  # fmt: skip
def test_invalid_settings_are_rejected_and_nothing_is_saved(device, fields):
    before = device.status()
    response = device.post("/api/pu/settings", {"deviceName": "must not be saved", **fields})
    assert response.status_code in {400, 403}
    assert response.json()["ok"] is False
    assert _comparable(device.status()) == _comparable(before)


def test_status_describes_address_and_access_point(device):
    status = device.status()
    address = status["address"]
    assert isinstance(address["static"], bool)
    assert address["current"]["ip"] == status["wifi"]["ip"]
    assert address["current"]["subnet"].count(".") == 3
    assert address["trialSeconds"] == 0
    ap = status["ap"]
    assert ap["open"] is False
    for values in (ap["active"], ap["defaults"]):
        assert values["mode"] in {"never", "failure", "always"}
        assert isinstance(values["passwordSet"], bool)
        assert values["timeout"] >= 0 and values["grace"] >= 0
        assert set(values) == {"mode", "timeout", "grace", "passwordSet"}  # never the password
    assert ap["own"]["password"] in {"default", "open", "custom"}


def test_confirm_and_revert_need_a_pending_address(device):
    assert device.post("/api/pu/network/confirm").status_code == 409
    assert device.post("/api/pu/network/revert").status_code == 409


def test_autoupdates_needs_its_parameter(device):
    response = device.post("/api/pu/autoupdates")
    assert response.status_code == 400
    assert response.json()["ok"] is False


def test_autoupdates_toggle_is_stored(device):
    original = device.status()["autoUpdates"]
    try:
        for value in (not original, original):
            response = device.post("/api/pu/autoupdates", {"enabled": str(value).lower()})
            assert response.status_code == 200
            assert device.status()["autoUpdates"] is value
    finally:
        device.post("/api/pu/autoupdates", {"enabled": str(original).lower()})


def test_install_of_malformed_version_fails_cleanly(device):
    # Installs run in the background, so the request is accepted and the result shows in status.
    response = device.post("/api/pu/update/install", {"version": "../../evil"})
    assert response.status_code == 202
    deadline = time.monotonic() + 5
    while device.status()["update"]["state"] != "failed" and time.monotonic() < deadline:
        time.sleep(0.2)
    update = device.status()["update"]
    assert update["state"] == "failed"
    assert update["message"]


def test_server_survives_parallel_requests(device):
    with ThreadPoolExecutor(max_workers=8) as pool:
        codes = list(pool.map(lambda _: device.get("/api/pu/status").status_code, range(40)))
    assert codes == [200] * 40


def test_open_device_warns_in_its_log(device):
    if device.auth:
        pytest.skip("device has an admin password")
    assert any("No admin password set" in line for line in device.log_lines())


@pytest.mark.parametrize("path", PROTECTED_GET)
def test_protected_pages_need_the_admin_login(device, path):
    if not device.auth:
        pytest.skip("device has no admin password; build with one and pass --admin-password")
    assert device.get(path, auth=False).status_code == 401
    assert device.get(path).status_code == 200


@pytest.mark.parametrize("path", MUTATING)
def test_protected_actions_need_the_admin_login(device, path):
    if not device.auth:
        pytest.skip("device has no admin password; build with one and pass --admin-password")
    assert device.post(path, auth=False).status_code == 401
