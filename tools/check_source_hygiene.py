#!/usr/bin/env python3
"""Small repository-owned source hygiene checker used by CI.

This is intentionally not a formatter. It enforces deterministic, low-risk
text invariants without adding a third-party formatting dependency.
"""

from __future__ import annotations

import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
CHECK_SUFFIXES = {".cpp", ".hpp", ".h", ".c", ".cmake", ".md", ".yml", ".yaml", ".py", ".txt"}
CHECK_NAMES = {"CMakeLists.txt", ".gitignore"}
SKIP_PARTS = {".git", "build", "build-sanitize", "artifacts", "reports"}


def should_check(path: pathlib.Path) -> bool:
    relative = path.relative_to(ROOT)
    if any(part in SKIP_PARTS or part.startswith("build-") for part in relative.parts):
        return False
    return path.name in CHECK_NAMES or path.suffix in CHECK_SUFFIXES


def check_file(path: pathlib.Path) -> list[str]:
    errors: list[str] = []
    raw = path.read_bytes()

    if b"\x00" in raw:
        return ["contains NUL byte"]

    try:
        text = raw.decode("utf-8")
    except UnicodeDecodeError:
        return ["is not valid UTF-8"]

    if text and not text.endswith("\n"):
        errors.append("missing final newline")

    for number, line in enumerate(text.splitlines(), start=1):
        if line.endswith(" ") or line.endswith("\t"):
            errors.append(f"line {number}: trailing whitespace")
        if "\t" in line and path.suffix in {".cpp", ".hpp", ".h", ".c"}:
            errors.append(f"line {number}: tab character in C/C++ source")

    return errors


def main() -> int:
    failures: list[str] = []
    for path in sorted(ROOT.rglob("*")):
        if not path.is_file() or not should_check(path):
            continue
        for message in check_file(path):
            failures.append(f"{path.relative_to(ROOT)}: {message}")

    if failures:
        print("Source hygiene check failed:", file=sys.stderr)
        for failure in failures:
            print(f" - {failure}", file=sys.stderr)
        return 1

    print("Source hygiene check passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
