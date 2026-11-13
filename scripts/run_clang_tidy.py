#!/usr/bin/env python3
"""Run clang-tidy over the library sources using the real ESP32 compile commands.

PlatformIO's own `pio check --tool clangtidy` does not pass the cross toolchain's system headers
to clang-tidy, which makes it misparse the code. This script instead:

1. lets PlatformIO export the compile commands of examples/basic,
2. keeps only the library's own sources,
3. adapts the commands for clang (xtensa GCC flags removed, toolchain headers added),
4. runs clang-tidy with the repository's .clang-tidy,
5. reports and fails on findings located in the library's own files. Diagnostics inside
   framework and third-party headers are dropped: clang cannot compile the xtensa inline
   assembly in a few ESP-IDF headers, which is irrelevant for checking this library.

Different clang-tidy versions report different findings, so CI and local runs use the version
pinned in CLANG_TIDY_VERSION (install it with `uv tool install clang-tidy==<version>`).

Usage: python3 scripts/run_clang_tidy.py [--env esp32dev] [--fix]
"""

from __future__ import annotations

import argparse
import json
import re
import shlex
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

CLANG_TIDY_VERSION = "23.1.4"  # keep in sync with .github/workflows/ci.yml
ROOT = Path(__file__).resolve().parent.parent
EXAMPLE = ROOT / "examples" / "basic"
SOURCES = ROOT / "src"

# Flags only the xtensa GCC understands.
GCC_ONLY_PREFIXES = (
    "-mlongcalls",
    "-mtext-section-literals",
    "-mdisable-hardware-atomics",
    "-fstrict-volatile-bitfields",
    "-fno-tree-switch-conversion",
    "-freorder-blocks",
    "-fipa-pta",
    "-fno-shrink-wrap",
    "-MMD",
)
# clang has no xtensa backend; any 32-bit little-endian target gives the right type sizes.
CLANG_ARGS = [
    "--target=riscv32-unknown-elf",
    "-U__riscv",
    "-D__XTENSA__",
    "-D__xtensa__",
    "-Wno-everything",
]


def toolchain_include_dirs(compiler: str) -> list[str]:
    """Ask the cross compiler where its C/C++ standard headers live."""
    result = subprocess.run(
        [compiler, "-E", "-x", "c++", "-", "-v"],
        input="",
        capture_output=True,
        text=True,
        check=True,
    )
    dirs: list[str] = []
    collecting = False
    for line in result.stderr.splitlines():
        if line.startswith("#include <...> search starts here"):
            collecting = True
        elif line.startswith("End of search list"):
            break
        elif collecting:
            dirs.append(str(Path(line.strip()).resolve()))
    return dirs


def adapt(entry: dict) -> dict:
    args = shlex.split(entry["command"])
    compiler, rest = args[0], args[1:]
    kept = [arg for arg in rest if not arg.startswith(GCC_ONLY_PREFIXES)]
    system = [
        flag for directory in toolchain_include_dirs(compiler) for flag in ("-isystem", directory)
    ]
    return {
        "directory": entry["directory"],
        "file": entry["file"],
        "arguments": ["clang++", *CLANG_ARGS, *system, *kept],
    }


DIAGNOSTIC = re.compile(r"^(?P<path>[^:\n]+):\d+:\d+: (?P<kind>warning|error): ")


def own_findings(output: str, base: Path) -> list[str]:
    """Split clang-tidy output into diagnostics and keep those located in the library sources."""
    blocks: list[tuple[bool, list[str]]] = []
    for line in output.splitlines():
        match = DIAGNOSTIC.match(line)
        if match:
            path = Path(match["path"])
            path = path if path.is_absolute() else base / path
            blocks.append((SOURCES in path.resolve().parents, [line]))
        elif blocks:
            blocks[-1][1].append(line)  # code snippet or note belonging to the diagnostic
    return ["\n".join(lines) for own, lines in blocks if own]


def source_path(entry: dict) -> Path:
    path = Path(entry["file"])
    return (path if path.is_absolute() else Path(entry["directory"]) / path).resolve()


def check_version() -> None:
    """Warn when the installed clang-tidy is not the pinned one: CI may then report differently."""
    output = subprocess.run(
        ["clang-tidy", "--version"], capture_output=True, text=True, check=False
    ).stdout
    match = re.search(r"version (\d+\.\d+\.\d+)", output)
    found = match.group(1) if match else "unknown"
    if found != CLANG_TIDY_VERSION:
        print(
            f"warning: clang-tidy {found} found, CI uses {CLANG_TIDY_VERSION}; "
            f"results may differ (uv tool install clang-tidy=={CLANG_TIDY_VERSION})",
            file=sys.stderr,
        )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--env", default="esp32dev", help="environment of examples/basic to use")
    parser.add_argument("--fix", action="store_true", help="apply suggested fixes")
    options = parser.parse_args()

    for tool in ("pio", "clang-tidy"):
        if shutil.which(tool) is None:
            print(f"error: {tool} not found on PATH", file=sys.stderr)
            return 2
    check_version()

    subprocess.run(
        ["pio", "run", "-d", str(EXAMPLE), "-e", options.env, "-t", "compiledb", "--silent"],
        check=True,
    )
    database = json.loads((EXAMPLE / "compile_commands.json").read_text(encoding="utf-8"))
    entries = [adapt(entry) for entry in database if SOURCES in source_path(entry).parents]
    if not entries:
        print("error: no library sources found in the compile database", file=sys.stderr)
        return 2

    with tempfile.TemporaryDirectory() as build_dir:
        (Path(build_dir) / "compile_commands.json").write_text(
            json.dumps(entries), encoding="utf-8"
        )
        command = [
            "clang-tidy",
            "-p",
            build_dir,
            f"--config-file={ROOT / '.clang-tidy'}",
            "--quiet",
        ]
        if options.fix:
            command.append("--fix")
        command += [entry["file"] for entry in entries]
        result = subprocess.run(command, check=False, capture_output=True, text=True)

    findings = own_findings(result.stdout, EXAMPLE)
    if findings:
        print("\n".join(findings))
        print(f"\nclang-tidy: {len(findings)} finding(s) in {len(entries)} files", file=sys.stderr)
        return 1
    if "error:" not in result.stdout and result.returncode != 0:
        # clang-tidy itself failed (bad config, crash) rather than reporting diagnostics
        print(result.stderr, file=sys.stderr)
        return result.returncode
    print(f"clang-tidy: no findings in {len(entries)} files")
    return 0


if __name__ == "__main__":
    sys.exit(main())
