"""Tests that restart the device or write settings. Run with --disruptive."""

from __future__ import annotations

import time

import pytest

pytestmark = pytest.mark.disruptive


def test_restart_request_is_answered_before_the_device_goes_down(device):
    before = device.boot_number()
    status = device.status()

    response = device.post("/api/pu/restart")
    assert response.status_code == 202
    assert response.json()["ok"] is True

    assert device.wait_for_boot(before) == before + 1  # also proves the counter is persisted
    after = device.status()
    assert after["wifi"]["connected"] is True
    assert after["wifi"]["portal"] is False
    assert after["deviceName"] == status["deviceName"]
    assert after["wifi"]["ssid"] == status["wifi"]["ssid"]


def test_saved_settings_survive_the_restart_and_keep_wifi_credentials(device):
    original = device.status()
    new_name = original["deviceName"] + " renamed"
    boot = device.boot_number()
    try:
        # Only the name is sent, the way the page does while the password box is untouched:
        # WiFi name and password must stay as they are, otherwise the device would not come back.
        response = device.post("/api/pu/settings", {"deviceName": new_name})
        assert response.status_code == 200
        assert response.json()["ok"] is True

        boot = device.wait_for_boot(boot)
        changed = device.status()
        assert changed["deviceName"] == new_name
        assert changed["wifi"]["connected"] is True
        assert changed["wifi"]["ssid"] == original["wifi"]["ssid"]
        assert changed["wifi"]["passwordSet"] is True
    finally:
        device.post("/api/pu/settings", {"deviceName": original["deviceName"]})
        device.wait_for_boot(boot)
    assert device.status()["deviceName"] == original["deviceName"]


def _fixed_ip_fields(status: dict) -> dict:
    """The device's current address, entered as a fixed one: it comes back where it was."""
    current = status["address"]["current"]
    return {
        "staticIP": "true",
        "ip": current["ip"],
        "subnet": current["subnet"],
        "gateway": current["gateway"],
        "dns1": current["dns1"],
        "dns2": "",
    }


def _back_to_dhcp(device, boot: int) -> None:
    device.post("/api/pu/settings", {"staticIP": "false"})
    boot = device.wait_for_boot(boot)
    assert device.status()["address"]["static"] is False


def test_fixed_address_is_kept_once_confirmed(device):
    original = device.status()
    if original["address"]["static"]:
        pytest.skip("the device already uses a static address")
    boot = device.boot_number()
    try:
        response = device.post("/api/pu/settings", _fixed_ip_fields(original))
        assert response.status_code == 200
        assert response.json()["trial"] is True
        boot = device.wait_for_boot(boot)

        trying = device.status()["address"]
        assert trying["static"] is True
        assert 0 < trying["trialSeconds"] <= 120
        assert device.post("/api/pu/network/confirm").status_code == 200
        time.sleep(1)  # carried out by the device's loop
        assert device.status()["address"]["trialSeconds"] == 0

        device.post("/api/pu/restart")
        boot = device.wait_for_boot(boot)
        after = device.status()["address"]
        assert after["static"] is True
        assert after["trialSeconds"] == 0
    finally:
        _back_to_dhcp(device, boot)


def test_unconfirmed_fixed_address_is_reverted(device):
    original = device.status()
    if original["address"]["static"]:
        pytest.skip("the device already uses a static address")
    boot = device.boot_number()
    try:
        assert device.post("/api/pu/settings", _fixed_ip_fields(original)).json()["trial"] is True
        boot = device.wait_for_boot(boot)
        assert device.status()["address"]["trialSeconds"] > 0
        boot = device.wait_for_boot(boot, timeout=180)  # nobody confirms: back after two minutes
        after = device.status()["address"]
        assert after["static"] is False
        assert after["reverted"] is True  # the log line is gone: the log lives in RAM
    finally:
        if device.status()["address"]["static"]:
            _back_to_dhcp(device, boot)


def test_access_point_settings_are_stored_per_device(device):
    original = device.status()
    if original["ap"]["locked"]:
        pytest.skip("access point settings are fixed in this firmware")
    boot = device.boot_number()
    try:
        response = device.post(
            "/api/pu/settings",
            {"apMode": "never", "apTimeout": "600", "apGrace": "", "apPassMode": "custom",
             "apPass": "a long password"},
        )  # fmt: skip
        assert response.status_code == 200
        boot = device.wait_for_boot(boot)
        ap = device.status()["ap"]
        assert ap["own"] == {"mode": "never", "timeout": 600, "grace": -1, "password": "custom"}
        assert ap["active"]["mode"] == "never"
        assert ap["active"]["timeout"] == 600
        assert ap["active"]["grace"] == ap["defaults"]["grace"]
        assert ap["active"]["passwordSet"] is True
    finally:
        device.post(
            "/api/pu/settings",
            {"apMode": "default", "apTimeout": "", "apGrace": "", "apPassMode": "default"},
        )
        boot = device.wait_for_boot(boot)
    ap = device.status()["ap"]
    assert ap["own"] == {"mode": "default", "timeout": -1, "grace": -1, "password": "default"}
    assert ap["active"] == ap["defaults"]
