"""Web console at /webserial: input reaches the sketch, output reaches the browser."""

from __future__ import annotations


def test_console_page_is_served(device):
    response = device.get("/webserial")
    assert response.status_code == 200
    assert "html" in response.headers["Content-Type"]


def test_reply_arrives_without_further_input(console):
    # Regression: output used to stay buffered on the device until the next message came in.
    console.send("hello")
    assert console.rows(1, timeout=2) == ["You said: hello"]


def test_each_line_is_one_row_without_line_breaks(console):
    console.send("first")
    console.send("second")
    rows = console.drain()
    assert rows == ["You said: first", "You said: second"]


def test_browser_style_terminators_are_stripped_from_input(console):
    # The stock web console page sends the text NUL-terminated.
    console.send(b"padded\x00")
    console.send(b"newline\r\n")
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
        assert console.rows(1, timeout=2) == ["You said: shared"]
        assert second.rows(1, timeout=2) == ["You said: shared"]


def test_http_api_stays_responsive_during_console_traffic(device, console):
    for index in range(10):
        console.send(f"busy{index}")
        assert device.get("/api/pu/status").status_code == 200
    assert len(console.drain(quiet=1.0)) == 10
