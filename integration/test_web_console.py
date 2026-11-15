"""Web console at /console: input reaches the sketch, output reaches the browser."""

from __future__ import annotations


def test_console_page_is_served(device):
    response = device.get("/console")
    assert response.status_code == 200
    assert "html" in response.headers["Content-Type"]


def test_old_console_address_redirects(device):
    response = device.get("/webserial", allow_redirects=False)
    assert response.status_code == 302
    assert response.headers["Location"] == "/console"


def test_reply_arrives_without_further_input(console):
    # Regression: output used to stay buffered on the device until the next message came in.
    console.send("hello")
    assert console.lines(1, timeout=2) == ["You said: hello"]


def test_each_line_arrives_once_without_line_breaks(console):
    console.send("first")
    console.send("second")
    assert console.drain() == ["You said: first", "You said: second"]


def test_line_terminators_are_stripped_from_input(console):
    console.send("padded\x00")
    console.send("newline\r\n")
    assert console.drain() == ["You said: padded", "You said: newline"]


def test_log_command_prints_the_same_log_as_the_api(device, console):
    expected = device.log_lines()
    console.send("log")
    assert console.drain(quiet=1.0) == expected


def test_burst_of_messages_is_answered_completely_and_in_order(console):
    words = [f"msg{index:02d}" for index in range(20)]
    for word in words:
        console.send(word)
    assert console.drain(quiet=1.0) == [f"You said: {word}" for word in words]


def test_two_consoles_both_receive_output(device, console):
    with device.web_console() as second:
        second.drain(quiet=0.3)
        console.send("shared")
        assert console.lines(1, timeout=2) == ["You said: shared"]
        assert second.lines(1, timeout=2) == ["You said: shared"]


def test_newly_opened_console_shows_recent_output(device, console):
    console.send("remember me")
    assert console.lines(1, timeout=2) == ["You said: remember me"]
    with device.web_console() as late:
        assert "You said: remember me" in late.drain(quiet=0.5)


def test_http_api_stays_responsive_during_console_traffic(device, console):
    for index in range(10):
        console.send(f"busy{index}")
        assert device.get("/api/pu/status").status_code == 200
    assert len(console.drain(quiet=1.0)) == 10


def test_console_accepts_the_token_from_the_api_instead_of_a_login(device):
    token = device.json("/api/pu/console/token")["token"]
    with device.web_console(token=token, auth=False) as console:
        console.drain(quiet=0.3)
        console.send("token")
        assert console.lines(1, timeout=2) == ["You said: token"]
