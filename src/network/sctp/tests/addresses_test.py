#!/usr/bin/env python3
"""Test SCTP address resolution/packing with the project's actual Address class.

Pass a native build directory containing compile_commands.json.
"""
import json
import pathlib
import shlex
import subprocess
import sys
import tempfile

root = pathlib.Path(__file__).resolve().parents[4]
build = pathlib.Path(sys.argv[1]).resolve()
commands = json.loads((build / "compile_commands.json").read_text())
entry = next(item for item in commands
             if item["file"] == str(root / "src/network/address.cpp"))
arguments = shlex.split(entry["command"])
flags = []
i = 1
while i < len(arguments):
    flag = arguments[i]
    if flag in ("-I", "-isystem", "-D"):
        flags.extend(arguments[i:i + 2])
        i += 2
        continue
    if flag.startswith(("-I", "-D")) and flag != "-DNDEBUG":
        flags.append(flag)
    i += 1
with tempfile.TemporaryDirectory(prefix="sctp-addresses-") as temporary:
    binary = pathlib.Path(temporary) / "test"
    subprocess.run(
        [arguments[0], *flags, "-std=c++20", "-ffunction-sections",
         "-fdata-sections", "-Wl,--gc-sections", str(root / "src/network/address.cpp"),
         str(root / "src/network/sctp/tests/addresses_test.cpp"), "-o", str(binary)],
        check=True)
    subprocess.run([str(binary)], check=True)
