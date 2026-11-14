"""The memory page's API: listing, changing, resetting, adding and deleting stored values."""

from __future__ import annotations

import pytest

LIBRARY = "project_utils"
AREA = "app"  # the example's own settings


def set_value(device, area: str, key: str, value: str):
    return device.post("/api/pu/memory/set", {"area": area, "key": key, "value": value})


def reset(device, area: str, key: str):
    return device.post("/api/pu/memory/reset", {"area": area, "key": key})


def memory(device) -> dict:
    return device.json("/api/pu/memory")


def entry(device, area: str, key: str) -> dict | None:
    for listed in memory(device)["areas"]:
        if listed["name"] == area:
            return next((item for item in listed["entries"] if item["key"] == key), None)
    return None


def test_lists_library_and_project_areas(device):
    result = memory(device)
    assert 0 < result["used"] <= result["total"]
    names = [area["name"] for area in result["areas"]]
    assert names[:2] == [LIBRARY, AREA]
    library = result["areas"][0]
    assert library["library"] is True
    keys = {item["key"]: item for item in library["entries"]}
    assert keys["ssid"]["defined"] is True and keys["ssid"]["type"] == "text"


def test_secrets_never_leave_the_device(device):
    for area in memory(device)["areas"]:
        for item in area["entries"]:
            if item["secret"]:
                assert "value" not in item and "default" not in item
                assert isinstance(item["set"], bool)
    assert entry(device, LIBRARY, "password")["secret"] is True
    assert entry(device, AREA, "apiKey")["secret"] is True  # declared with Page::password()


@pytest.mark.parametrize(
    ("fields", "status"),
    [
        ({"area": LIBRARY, "key": "new", "type": "int", "value": "1"}, 403),
        ({"area": AREA, "key": "brightness", "type": "int", "value": "1"}, 409),
        ({"area": AREA, "key": "bad name!", "type": "int", "value": "1"}, 400),
        ({"area": AREA, "key": "sixteen-chars-xx", "type": "int", "value": "1"}, 400),
        ({"area": AREA, "key": "tmp", "type": "int", "value": "1.5"}, 400),
        ({"area": AREA, "key": "tmp", "type": "bool", "value": "maybe"}, 400),
        ({"area": AREA, "key": "tmp", "type": "list", "value": "1"}, 400),
        ({"area": "nvs.net80211", "key": "tmp", "type": "int", "value": "1"}, 409),
    ],
    ids=["library-area", "exists", "bad-name", "long-name", "not-whole", "not-bool",
         "bad-type", "foreign-area"],
)  # fmt: skip
def test_invalid_additions_are_refused(device, fields, status):
    response = device.post("/api/pu/memory/add", fields)
    assert response.status_code == status
    assert entry(device, AREA, "tmp") is None


def test_wrong_values_are_refused(device):
    before = entry(device, AREA, "brightness")
    assert (
        device.post(
            "/api/pu/memory/set", {"area": AREA, "key": "brightness", "value": "x"}
        ).status_code
        == 400
    )
    assert (
        device.post("/api/pu/memory/set", {"area": "nowhere", "key": "a", "value": "1"}).status_code
        == 404
    )
    assert device.post("/api/pu/memory/reset", {"area": AREA, "key": "missing"}).status_code == 404
    assert entry(device, AREA, "brightness") == before


@pytest.mark.disruptive
def test_change_and_reset_a_defined_value(device):
    original = entry(device, AREA, "brightness")
    try:
        response = device.post(
            "/api/pu/memory/set", {"area": AREA, "key": "brightness", "value": "77"}
        )
        assert response.status_code == 200
        changed = entry(device, AREA, "brightness")
        assert changed["value"] == 77 and changed["stored"] is True
        assert (
            device.post("/api/pu/memory/reset", {"area": AREA, "key": "brightness"}).status_code
            == 200
        )
        reset = entry(device, AREA, "brightness")
        assert reset["stored"] is False  # gone from flash...
        assert reset["value"] == reset["default"]  # ...and the default applies right away
    finally:
        if original["stored"]:
            device.post(
                "/api/pu/memory/set",
                {"area": AREA, "key": "brightness", "value": str(original["value"])},
            )


@pytest.mark.disruptive
@pytest.mark.parametrize(
    ("kind", "value", "expected", "changed", "expected_change"),
    [("int", "-12", -12, "40", 40), ("float", "2.5", 2.5, "0.1", 0.1),
     ("bool", "true", True, "false", False), ("text", " spaced ", " spaced ", "", "")],
)  # fmt: skip
def test_add_change_and_delete_an_entry(device, kind, value, expected, changed, expected_change):
    key = "pytest-" + kind
    try:
        add = {"area": AREA, "key": key, "type": kind, "value": value}
        assert device.post("/api/pu/memory/add", add).status_code == 200
        added = entry(device, AREA, key)
        assert added["defined"] is False and added["stored"] is True
        assert added["type"] == kind and added["value"] == expected
        set_ = {"area": AREA, "key": key, "value": changed}
        assert device.post("/api/pu/memory/set", set_).status_code == 200
        assert entry(device, AREA, key)["value"] == expected_change
    finally:
        device.post("/api/pu/memory/reset", {"area": AREA, "key": key})
    assert entry(device, AREA, key) is None


@pytest.mark.disruptive
def test_new_area_is_listed_until_its_last_entry_goes(device):
    fields = {"area": "pytest-area", "key": "level", "type": "int", "value": "3"}
    try:
        assert device.post("/api/pu/memory/add", fields).status_code == 200
        assert entry(device, "pytest-area", "level")["value"] == 3
    finally:
        device.post("/api/pu/memory/reset", {"area": "pytest-area", "key": "level"})
    assert "pytest-area" not in [area["name"] for area in memory(device)["areas"]]


@pytest.mark.disruptive
def test_values_survive_a_restart(device):
    key = "pytest-keep"
    boot = device.boot_number()
    try:
        add = {"area": AREA, "key": key, "type": "int", "value": "5"}
        assert device.post("/api/pu/memory/add", add).status_code == 200
        device.post("/api/pu/restart")
        device.wait_for_boot(boot)
        assert entry(device, AREA, key)["value"] == 5
    finally:
        device.post("/api/pu/memory/reset", {"area": AREA, "key": key})
