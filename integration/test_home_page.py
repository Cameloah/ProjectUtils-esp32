"""The example's home page, declared in C++ with pu::Page."""

from __future__ import annotations

import time

import pytest
from device import HomePage


@pytest.fixture
def home(device):
    page = HomePage(device)
    yield page
    page.act("Motor", "release")  # leave lamp and motor off for the next test
    page.act("Stop everything", "click")


def test_layout_lists_the_widgets_in_declared_order(home):
    assert home.layout["title"] == "My ESP32"
    kinds = [widget["kind"] for widget in home.layout["widgets"]]
    assert kinds == [
        "section", "text", "number", "slider", "toggle", "select",
        "section", "toggle", "slider", "hold", "button", "button",
        "section", "value", "value", "value", "indicator", "indicator",
    ]  # fmt: skip
    interval = home.layout["widgets"][home.index("Interval")]
    assert interval == {
        "kind": "number", "label": "Interval", "unit": "s", "min": 0.1, "max": 60, "step": 0.1,
    }  # fmt: skip
    assert home.layout["widgets"][home.index("Stop everything")]["color"] == "red"
    assert home.layout["widgets"][home.index("Speed")]["options"] == ["slow", "normal", "fast"]
    assert len(home.layout["values"]) == len(kinds)


def test_unknown_page_and_widget_are_reported(device, home):
    assert device.get("/api/pu/page?path=/nope").status_code == 404
    response = device.post("/api/pu/page/widget", {"path": "/", "widget": 99, "action": "click"})
    assert response.status_code == 404
    assert home.act("Clicks", "click").status_code == 400  # a live value cannot be clicked
    assert home.act("Count a click", "set", 1).status_code == 400


def test_button_runs_its_callback_once_per_click(home):
    before = int(home.value("Clicks"))
    for _click in range(3):
        assert home.act("Count a click", "click").status_code == 200
    assert home.wait_for("Clicks", str(before + 3)) == str(before + 3)


def test_callback_toggle_and_indicator_follow_each_other(home):
    assert home.act("Lamp", "set", "true", kind="toggle").status_code == 200
    assert home.wait_for("Lamp", True, kind="indicator") is True
    assert home.value("Lamp", kind="toggle") is True
    assert home.act("Lamp", "set", "false", kind="toggle").status_code == 200
    assert home.wait_for("Lamp", False, kind="indicator") is False
    assert home.act("Lamp", "set", "maybe", kind="toggle").status_code == 400


def test_callback_slider_checks_its_range(home):
    assert home.act("Volume", "set", 70).status_code == 200
    assert home.wait_for("Volume", 70) == 70
    assert home.act("Volume", "set", 101).status_code == 400
    assert home.act("Volume", "set", "loud").status_code == 400
    assert home.value("Volume") == 70


def test_immediate_button_switches_everything_off(home):
    home.act("Lamp", "set", "true", kind="toggle")
    home.act("Motor", "press")
    assert home.wait_for("Lamp", True, kind="indicator") is True
    assert home.act("Stop everything", "click").status_code == 200
    assert home.wait_for("Lamp", False, kind="indicator") is False
    assert home.wait_for("Motor running", False) is False


def test_hold_button_is_active_only_while_kept_alive(home):
    assert home.act("Motor", "press").status_code == 200
    assert home.wait_for("Motor running", True) is True
    for _beat in range(6):  # 1.8 s of keep-alives: longer than the 1 s timeout
        time.sleep(0.3)
        assert home.act("Motor", "hold").status_code == 200
    assert home.value("Motor running") is True
    assert home.value("Motor") is True
    assert home.act("Motor", "release").status_code == 200
    assert home.wait_for("Motor running", False) is False


def test_hold_button_lets_go_by_itself_when_the_browser_disappears(home):
    home.act("Motor", "press")
    assert home.wait_for("Motor running", True) is True
    time.sleep(0.5)
    assert home.value("Motor running") is True  # not too early
    assert home.wait_for("Motor running", False, timeout=2.5) is False  # safety timeout


def test_bound_inputs_validate_and_store_settings(home):
    original = {
        label: home.value(label) for label in ("Interval", "Brightness", "Night mode", "Speed")
    }
    original["Greeting"] = home.value("Greeting", kind="text")
    try:
        assert home.act("Interval", "set", "2.5").status_code == 200
        assert home.value("Interval") == 2.5
        for bad in ("0", "61", "abc", "", "1e999"):
            assert home.act("Interval", "set", bad).status_code == 400, bad
        assert home.value("Interval") == 2.5

        assert home.act("Brightness", "set", 80).status_code == 200
        assert home.value("Brightness") == 80
        assert home.act("Brightness", "set", "80.5").status_code == 400  # whole numbers only
        assert home.act("Brightness", "set", 101).status_code == 400

        assert home.act("Night mode", "set", "true").status_code == 200
        assert home.value("Night mode") is True

        assert home.act("Speed", "set", "fast").status_code == 200
        assert home.value("Speed") == "fast"
        assert home.act("Speed", "set", "warp").status_code == 400

        assert home.act("Greeting", "set", "moin", kind="text").status_code == 200
        assert home.wait_for("Greeting", "moin", kind="value") == "moin"  # live value follows
        assert home.act("Greeting", "set", "x" * 201, kind="text").status_code == 400
    finally:
        home.act("Interval", "set", original["Interval"])
        home.act("Brightness", "set", original["Brightness"])
        home.act("Night mode", "set", str(original["Night mode"]).lower())
        home.act("Speed", "set", original["Speed"])
        home.act("Greeting", "set", original["Greeting"], kind="text")


@pytest.mark.disruptive
def test_bound_setting_survives_a_restart(device, home):
    original = home.value("Brightness")
    changed = 17 if original != 17 else 18
    boot = device.boot_number()
    try:
        assert home.act("Brightness", "set", changed).status_code == 200
        assert device.post("/api/pu/restart").status_code == 202
        boot = device.wait_for_boot(boot)
        assert HomePage(device).value("Brightness") == changed
    finally:
        HomePage(device).act("Brightness", "set", original)
