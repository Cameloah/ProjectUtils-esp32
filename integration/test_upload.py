"""Firmware upload from the browser (update page)."""

from __future__ import annotations

from pathlib import Path

import pytest

REPO = Path(__file__).resolve().parent.parent
UPLOAD = "/api/pu/update/upload"


def upload(device, content: bytes, query: str = "", **kwargs):
    files = {"file": ("firmware.bin", content, "application/octet-stream")}
    return device.post(UPLOAD + query, files=files, timeout=120, **kwargs)


def test_upload_without_a_file_is_rejected(device):
    response = device.post(UPLOAD)
    assert response.status_code == 400
    assert response.json()["ok"] is False


def test_file_that_is_not_firmware_is_rejected_and_the_device_keeps_running(device):
    boot = device.boot_number()
    response = upload(device, b"this is not a firmware image\n" * 400)
    assert response.status_code == 400
    assert response.json()["message"]
    assert device.boot_number() == boot
    assert any("Upload failed" in line for line in device.log_lines())


def test_file_larger_than_the_firmware_partition_is_rejected_before_writing(device):
    response = upload(device, b"\xe9" + b"\x00" * 1023, "?size=99999999")
    assert response.status_code == 400
    assert "larger" in response.json()["message"]


def test_upload_with_wrong_checksum_is_rejected(device):
    response = upload(device, b"\xe9" + b"\x00" * 8191, "?md5=" + "0" * 32)
    assert response.status_code == 400


def test_a_second_upload_works_after_a_failed_one(device):
    assert upload(device, b"garbage" * 1000).status_code == 400
    assert upload(device, b"garbage" * 1000).status_code == 400
    assert device.status()["wifi"]["connected"] is True


@pytest.mark.disruptive
def test_uploaded_firmware_is_installed_and_started(device, pytestconfig):
    env = pytestconfig.getoption("--flash") or "esp32s3"
    image = REPO / "examples" / "basic" / ".pio" / "build" / env / "firmware.bin"
    if not image.exists():
        pytest.skip(f"{image} not built; run with --flash {env} once")
    boot = device.boot_number()
    content = image.read_bytes()

    response = upload(device, content, f"?size={len(content)}")
    assert response.status_code == 200, response.text
    assert response.json()["ok"] is True

    assert device.wait_for_boot(boot) == boot + 1
    assert device.status()["wifi"]["connected"] is True
