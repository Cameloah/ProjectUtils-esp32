"""Tests that restart the device or write settings. Run with --disruptive."""

from __future__ import annotations

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
        # Only the name is sent, the way the page sends an empty password field: WiFi name and
        # password must stay as they are, otherwise the device would not come back.
        response = device.post("/api/pu/settings", {"deviceName": new_name, "pass": ""})
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
