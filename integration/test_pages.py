"""The built-in pages, the shared menu, device info and the WiFi search."""

from __future__ import annotations

import time

import pytest

BUILT_IN = ["/settings", "/update", "/console", "/info"]


@pytest.mark.parametrize("path", ["/", "/about", *BUILT_IN])
def test_every_page_is_complete_and_uses_the_shared_shell(device, path):
    page = device.get(path)
    assert page.status_code == 200
    assert page.headers["Content-Type"].startswith("text/html")
    assert page.text.rstrip().endswith("</html>")  # not cut off
    assert 'href="/pu/app.css"' in page.text
    assert 'src="/pu/app.js"' in page.text
    assert "http://" not in page.text and "https://" not in page.text  # works without internet


def test_shared_style_and_script_are_served_without_login(device):
    style = device.get("/pu/app.css", auth=False)
    assert style.status_code == 200
    assert style.headers["Content-Type"].startswith("text/css")
    assert ".menu__box" in style.text

    script = device.get("/pu/app.js", auth=False)
    assert script.status_code == 200
    assert "javascript" in script.headers["Content-Type"]
    assert "/api/pu/nav" in script.text


def test_menu_lists_project_pages_first_then_the_built_in_ones(device):
    nav = device.json("/api/pu/nav")
    assert nav["deviceName"] == device.status()["deviceName"]
    assert nav["project"] == [
        {"path": "/", "title": "My ESP32"},
        {"path": "/about", "title": "About"},
    ]
    assert [entry["path"] for entry in nav["device"]] == BUILT_IN
    for entry in nav["project"] + nav["device"]:
        assert device.get(entry["path"]).status_code == 200, entry


def test_device_info_is_plausible(device):
    info = device.json("/api/pu/info")
    assert info["chip"].startswith("ESP32")
    assert info["connected"] is True
    assert info["ip"] == device.status()["wifi"]["ip"]
    assert -100 < info["rssi"] < 0
    assert len(info["mac"].split(":")) == 6
    assert 10_000 < info["heapMinFree"] <= info["heapFree"] < info["heapSize"]
    assert info["sketchSize"] > 500_000
    assert info["sketchFree"] > 0
    assert info["flashSize"] >= 4 * 1024 * 1024
    assert info["resetReason"]
    assert info["time"].startswith("20")

    time.sleep(1.2)
    assert device.json("/api/pu/info")["uptimeSeconds"] > info["uptimeSeconds"]


def test_wifi_search_finds_the_network_the_device_is_on(device):
    own = device.status()["wifi"]["ssid"]
    networks: list[dict] = []
    for _attempt in range(3):  # a search can miss a network now and then
        assert device.post("/api/pu/wifi/scan", timeout=15).status_code == 202
        deadline = time.monotonic() + 20
        scan = {"scanning": True}
        while scan["scanning"] and time.monotonic() < deadline:
            time.sleep(0.5)
            # the radio is busy on other channels while searching, so answers can be slow
            scan = device.get("/api/pu/wifi/scan", timeout=15).json()
        assert scan["scanning"] is False
        networks = scan["networks"]
        if own in [network["ssid"] for network in networks]:
            break
    names = [network["ssid"] for network in networks]
    assert own in names
    assert len(names) == len(set(names)), "each network should be listed once"
    for network in networks:
        assert network["ssid"]
        assert -110 < network["rssi"] < 0
        assert isinstance(network["secure"], bool)
    assert device.status()["wifi"]["connected"] is True  # searching did not drop the connection
