"""The pages in a real (headless) browser: scripts run, the menu works, controls reach the device.

Needs Playwright, which is optional:
    uv run --with playwright playwright install chromium-headless-shell
and add `--with playwright` to the test command. Without it these tests are skipped.
"""

from __future__ import annotations

import pytest

playwright = pytest.importorskip("playwright.sync_api", reason="Playwright is not installed")

PAGES = ["/", "/about", "/settings", "/update", "/console", "/memory", "/info"]


@pytest.fixture(scope="module")
def browser():
    with playwright.sync_playwright() as api:
        try:
            instance = api.chromium.launch()
        except playwright.Error as error:
            pytest.skip(f"no browser for Playwright: {error}")
        yield instance
        instance.close()


@pytest.fixture
def page(browser, device):
    credentials = {"username": device.auth[0], "password": device.auth[1]} if device.auth else None
    context = browser.new_context(
        base_url=device.base, http_credentials=credentials, viewport={"width": 420, "height": 800}
    )
    tab = context.new_page()
    tab.problems = []
    tab.on("pageerror", lambda error: tab.problems.append(str(error)))
    tab.on(
        "console",
        lambda message: tab.problems.append(message.text) if message.type == "error" else None,
    )
    yield tab
    context.close()


def row(page, label: str):
    """The row of a home page control, found by its label."""
    return page.locator(".row", has=page.locator(".label", has_text=label))


def tile(page, label: str):
    """The tile of a live value or status light, found by its label."""
    return page.locator(".tile", has=page.locator(".label", has_text=label))


def wait_for_menu(page):
    """The menu starts with the built-in pages and gains the project's once loaded."""
    page.wait_for_function("document.querySelectorAll('.pu-rail a').length === 7")


@pytest.mark.parametrize("path", PAGES)
def test_page_loads_without_script_errors_and_shows_the_menu(page, path):
    page.goto(path)
    wait_for_menu(page)
    titles = [title.strip() for title in page.locator(".pu-rail a").all_inner_texts()]
    assert titles == ["My ESP32", "About", "Settings", "Update", "Console", "Memory", "Device info"]
    assert page.locator(".pu-rail a[aria-current=page]").get_attribute("href") == path
    page.wait_for_timeout(1500)  # let the page fetch its data
    assert page.problems == []


def test_menu_is_a_drawer_on_a_phone_and_navigates(page):
    page.goto("/")
    wait_for_menu(page)
    rail = page.locator(".pu-rail")
    assert rail.bounding_box()["x"] < 0  # closed: parked left of the screen
    page.click(".pu-burger")
    page.wait_for_function("el => el.getBoundingClientRect().x === 0", arg=rail.element_handle())
    page.locator(".pu-rail a", has_text="Device info").click()
    page.wait_for_url("**/info")
    page.locator("dl.kv dt", has_text="MAC address").wait_for()


def test_menu_is_always_visible_on_a_wide_screen(page):
    page.set_viewport_size({"width": 1200, "height": 800})
    page.goto("/")
    wait_for_menu(page)
    assert page.locator(".pu-rail").bounding_box()["x"] == 0
    assert not page.locator(".pu-burger").is_visible()
    heading = page.locator("h1").bounding_box()
    assert heading["x"] >= page.locator(".pu-rail").bounding_box()["width"]  # not underneath


def test_root_shows_the_project_home_page(page):
    page.goto("/")
    page.locator(".eyebrow", has_text="Live values").wait_for()
    assert page.title() == "My ESP32"
    assert page.inner_text("h1") == "My ESP32"


def test_home_button_updates_the_live_value(page):
    page.goto("/")
    clicks = tile(page, "Clicks").locator(".value")
    clicks.wait_for()
    page.wait_for_function("el => el.textContent !== ''", arg=clicks.element_handle())
    before = int(clicks.inner_text())
    page.click("button:has-text('Count a click')")
    page.wait_for_function(
        "([el, expected]) => el.textContent === expected",
        arg=[clicks.element_handle(), str(before + 1)],
    )
    assert page.problems == []


def test_home_toggle_lights_the_indicator_and_stop_clears_it(page):
    page.goto("/")
    page.locator(".eyebrow", has_text="Live values").wait_for()
    row(page, "Lamp").locator(".switch").click()
    dot = tile(page, "Lamp").locator(".lamp")
    page.wait_for_function("el => el.classList.contains('on')", arg=dot.element_handle())
    page.click("button:has-text('Stop everything')")
    page.wait_for_function("el => !el.classList.contains('on')", arg=dot.element_handle())
    assert page.problems == []


def test_home_hold_button_runs_only_while_pressed(page):
    page.goto("/")
    page.locator(".eyebrow", has_text="Live values").wait_for()
    button = row(page, "Motor").locator("button")
    dot = tile(page, "Motor running").locator(".lamp")
    button.hover()
    page.mouse.down()
    page.wait_for_function("el => el.classList.contains('on')", arg=dot.element_handle())
    page.wait_for_timeout(1800)  # longer than the device's 1 s timeout: keep-alives must work
    assert "on" in dot.get_attribute("class")
    page.mouse.up()
    page.wait_for_function("el => !el.classList.contains('on')", arg=dot.element_handle())
    assert page.problems == []


def test_home_number_input_is_saved_and_rejects_bad_values(page, device):
    from device import HomePage

    home = HomePage(device)
    original = home.value("Interval")
    page.goto("/")
    field = row(page, "Interval").locator("input")
    field.wait_for()
    try:
        field.fill("3.5")
        field.press("Enter")
        page.wait_for_function("el => el.classList.contains('saved')", arg=field.element_handle())
        assert home.value("Interval") == 3.5

        field.fill("500")  # above the maximum of 60
        field.press("Enter")
        page.wait_for_function("el => el.classList.contains('invalid')", arg=field.element_handle())
        page.locator("#pu-toast.error").wait_for()
        assert home.value("Interval") == 3.5
    finally:
        home.act("Interval", "set", original)


def test_console_page_sends_and_shows_the_reply(page):
    page.goto("/console")
    page.locator("#state", has_text="Connected").wait_for()
    page.fill("#command", "from the browser")
    page.press("#command", "Enter")
    page.locator("#output div", has_text="You said: from the browser").wait_for()
    assert page.locator("#output .sent", has_text="> from the browser").count() == 1
    assert page.problems == []


def test_settings_page_shows_current_values_and_no_password(page, device):
    status = device.status()
    page.goto("/settings")
    page.wait_for_function("document.getElementById('ssid').value !== ''")
    assert page.input_value("#deviceName") == status["deviceName"]
    assert page.input_value("#ssid") == status["wifi"]["ssid"]
    assert page.input_value("#pass") == "\u2022" * 8  # a stand-in, never the password


def test_settings_page_lists_networks_and_fills_in_the_chosen_one(page, device):
    page.goto("/settings")
    page.wait_for_function("document.getElementById('ssid').value !== ''")
    page.click("#scan")
    page.locator("#networks .net").first.wait_for(timeout=40000)
    own = device.status()["wifi"]["ssid"]
    entry = page.locator("#networks .net", has_text=own).first
    if entry.count() == 0:
        pytest.skip("own network not in this search result")
    page.fill("#ssid", "")
    entry.click()
    assert page.input_value("#ssid") == own


def test_settings_page_shows_address_and_access_point(page, device):
    status = device.status()
    page.goto("/settings")
    page.wait_for_function("document.getElementById('apMode').options.length > 0")
    assert page.is_checked("#staticIP") == status["address"]["static"]
    assert page.is_hidden("#ipFields") != status["address"]["static"]
    if not status["address"]["static"]:
        page.check("#staticIP")  # switching on prefills what the router handed out
        assert page.input_value("#ip") == status["address"]["current"]["ip"]
        assert page.input_value("#subnet") == status["address"]["current"]["subnet"]
    own = status["ap"]["own"]["mode"]
    assert page.input_value("#apMode") == (
        status["ap"]["defaults"]["mode"] if own == "default" else own
    )
    page.check("#apPassOn")
    assert page.is_visible("#apPass")
    page.select_option("#apMode", "always")
    assert page.is_hidden("#apGraceRow")
    page.select_option("#apMode", "failure")
    assert page.is_visible("#apGraceRow")
    page.click("#apReset")
    assert page.input_value("#apMode") == status["ap"]["defaults"]["mode"]
    assert page.is_checked("#apPassOn") == status["ap"]["defaults"]["passwordSet"]
    assert page.is_hidden("#adminFields") != status["admin"]["set"]
    assert page.problems == []


def test_wrap_toggle_is_off_by_default_and_remembered(page):
    page.goto("/console")
    output = page.locator("#output")
    assert page.is_checked("#wrap") is False
    assert "nowrap" in output.get_attribute("class")
    page.check("#wrap")
    assert "nowrap" not in output.get_attribute("class")
    page.goto("/info")
    assert page.is_checked("#wrap") is True  # one setting for console and log
    assert "nowrap" not in page.locator("#log").get_attribute("class")
    assert page.problems == []


def test_memory_page_lists_entries_with_reset_and_delete_buttons(page, device):
    page.goto("/memory")
    page.locator(".entry").first.wait_for()
    assert (
        page.locator(".eyebrow", has_text="ProjectUtils library").count() == 0
    )  # hidden by default
    page.check("#showLibrary")
    page.locator(".eyebrow", has_text="ProjectUtils library").wait_for()
    password = page.locator(".entry", has=page.locator(".key", has_text="password")).first
    assert password.locator("input").input_value() == ""  # secret: never shown
    assert page.input_value("#addArea") == "app"
    assert "project_utils" not in page.locator("#addArea option").all_inner_texts()
    assert page.problems == []


def test_console_uses_the_whole_width(browser, device):
    credentials = {"username": device.auth[0], "password": device.auth[1]} if device.auth else None
    context = browser.new_context(
        base_url=device.base, http_credentials=credentials, viewport={"width": 1600, "height": 900}
    )
    tab = context.new_page()
    tab.goto("/console")
    box = tab.locator(".term").bounding_box()
    assert box["width"] > 1200  # not held to the 860 px column of the other pages
    assert box["y"] + box["height"] > 700  # reaches down towards the bottom of the window
    context.close()


def test_info_page_shows_device_data_and_log(page):
    page.goto("/info")
    page.locator("dl.kv dt", has_text="MAC address").wait_for()
    assert page.locator("dl.kv dt").count() >= 8
    assert page.inner_text("#uptime") != "-"
    assert int(page.inner_text("#heap")) > 50
    page.wait_for_function("document.getElementById('log').textContent.includes('Boot number')")


def test_info_page_restart_asks_before_restarting(page, device):
    boot = device.boot_number()
    page.goto("/info")
    page.click("#restart")
    page.locator("#restartYes").wait_for()
    page.click("#restartBox button:has-text('Cancel')")
    page.locator("#restart").wait_for()
    page.wait_for_timeout(2500)
    assert device.boot_number() == boot


def test_update_page_shows_version_and_hides_unconfigured_online_updates(page):
    page.goto("/update")
    page.locator("#firmwareInfo", has_text="Installed firmware: v").wait_for()
    assert page.locator("#githubCard").is_hidden()
    page.click("#upload")
    assert page.inner_text("#uploadMessage") == "Choose a file first."
