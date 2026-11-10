"""Compile host/button_icons.h (which ButtonPrompts set loads, host/mods.c) and check its choices.

python -I -B tests/test_button_icons.py [--cc clang]
"""
import argparse
import os
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cc", default="cc")
    args = parser.parse_args()
    process_flags = (subprocess.CREATE_NO_WINDOW | subprocess.BELOW_NORMAL_PRIORITY_CLASS) if os.name == "nt" else 0

    def run(command):
        result = subprocess.run(command, creationflags=process_flags, capture_output=True,
                                text=True, encoding="utf-8", errors="replace")
        print(result.stdout, end="")
        print(result.stderr, end="", file=sys.stderr)
        result.check_returncode()

    temporary = tempfile.TemporaryDirectory(prefix="button-icons-")
    folder = Path(temporary.name).resolve()
    try:
        executable = folder / "fixture.exe"
        run([args.cc, "-std=c11", "-Wall", "-Wextra", "-Werror", "-I" + str(ROOT / "host"),
             str(ROOT / "tests/button_icons_test.c"), "-o", str(executable)])
        run([str(executable)])
    finally:
        temporary.cleanup()


if __name__ == "__main__":
    main()
