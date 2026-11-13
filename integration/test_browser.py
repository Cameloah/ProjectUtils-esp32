"""The pages in a real (headless) browser: scripts run, the menu works, controls reach the device.

Needs Playwright, which is optional:
    uv run --with playwright playwright install chromium-headless-shell
and add `--with playwright` to the test command. Without it these tests are skipped.
"""

from __future__ import annotations

import pytest

playwright = pytest.importorskip("playwright.sync_api", reason="Playwright is not installed")

PAGES = ["/", "/about", "/settings", "/update", "/console", "/info"]


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
    """The row of a home page widget, found by its label."""
    return page.locator(".row", has=page.locator(".label", has_text=label))


@pytest.mark.parametrize("path", PAGES)
def test_page_loads_without_script_errors_and_shows_the_menu(page, path):
    page.goto(path)
    page.wait_for_selector(".menu__box a", state="attached")
    titles = page.locator(".menu__box a").all_inner_texts()
    assert titles == ["My ESP32", "About", "Settings", "Update", "Console", "Device info"]
    assert page.locator(".menu__item.active").get_attribute("href") == path
    page.wait_for_timeout(1500)  # let the page fetch its data
    assert page.problems == []


def test_menu_opens_and_navigates(page):
    page.goto("/")
    page.wait_for_selector(".menu__box a", state="attached")
    box = page.locator(".menu__box")
    assert box.bounding_box()["x"] < 0  # closed: parked left of the screen
    page.click(".menu__btn")
    page.wait_for_function("el => el.getBoundingClientRect().x === 0", arg=box.element_handle())
    link = page.locator(".menu__box a", has_text="Device info")
    link.click()
    page.wait_for_url("**/info")
    page.locator("table.info td", has_text="Free memory").wait_for()


def test_root_shows_the_project_home_page(page):
    page.goto("/")
    page.locator("h2", has_text="Live values").wait_for()
    assert page.title() == "My ESP32"
    assert page.inner_text("h1") == "My ESP32"


def test_home_button_updates_the_live_value(page):
    page.goto("/")
    clicks = row(page, "Clicks").locator(".value")
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
    page.locator("h2", has_text="Live values").wait_for()
    row(page, "Lamp").locator(".switch").click()
    dot = row(page, "Lamp").locator(".dot")
    page.wait_for_function("el => el.classList.contains('on')", arg=dot.element_handle())
    page.click("button:has-text('Stop everything')")
    page.wait_for_function("el => !el.classList.contains('on')", arg=dot.element_handle())
    assert page.problems == []


def test_home_hold_button_runs_only_while_pressed(page):
    page.goto("/")
    page.locator("h2", has_text="Live values").wait_for()
    button = row(page, "Motor").locator("button")
    dot = row(page, "Motor running").locator(".dot")
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
    assert page.input_value("#pass") == ""
    assert page.get_attribute("#pass", "placeholder") == "unchanged"


def test_settings_page_lists_networks_and_fills_in_the_chosen_one(page, device):
    page.goto("/settings")
    page.wait_for_function("document.getElementById('ssid').value !== ''")
    page.click("#scan")
    page.locator("#networks li").first.wait_for(timeout=40000)
    own = device.status()["wifi"]["ssid"]
    entry = page.locator("#networks li", has_text=own).first
    if entry.count() == 0:
        pytest.skip("own network not in this search result")
    page.fill("#ssid", "")
    entry.click()
    assert page.input_value("#ssid") == own


def test_info_page_shows_device_data_and_log(page):
    page.goto("/info")
    page.locator("table.info td", has_text="Free memory").wait_for()
    assert page.locator("table.info tr").count() >= 10
    page.wait_for_function("document.getElementById('log').textContent.includes('Boot number')")


def test_update_page_shows_version_and_hides_unconfigured_online_updates(page):
    page.goto("/update")
    page.locator("#firmwareInfo", has_text="Installed firmware: v").wait_for()
    assert page.locator("#githubCard").is_hidden()
    page.click("#upload")
    assert page.inner_text("#uploadMessage") == "Choose a file first."
