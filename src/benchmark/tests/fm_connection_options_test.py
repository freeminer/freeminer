#!/usr/bin/env python3
"""Run connection benchmark CLI regressions against a built freeminer binary."""
import pathlib
import re
import socket
import subprocess
import sys
import tempfile
import time

binary = pathlib.Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else (
    pathlib.Path(__file__).resolve().parents[3] / "build_-20/freeminer"
)

with tempfile.TemporaryDirectory(prefix="fm-benchmark-options-") as directory:
    def run(*args):
        return subprocess.run([str(binary), "--run-benchmarks", *args], cwd=directory,
                text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=90)

    for args in (
        ("--connection-benchmark-clients=0",),
        ("--connection-benchmark-clients=65536",),
        ("--connection-benchmark-clients=1,,2",),
        ("--connection-benchmark-clients=-1",),
        ("--connection-benchmark-sizes=0",),
        ("--connection-benchmark-sizes=4294967294",),
        ("--connection-benchmark-sizes=64,",),
        ("--connection-benchmark-sizes=1KiB",),
        ("--connection-benchmark-sizes=-0",),
        ("--connection-benchmark-sizes=-4294967293",),
        ("--connection-benchmark-reliability=maybe",),
    ):
        result = run(*args, "--help")
        assert result.returncode != 0 and "Invalid --connection-benchmark-" in result.stdout, result.stdout

    result = run("--connection-benchmark-clients=1,65535",
            "--connection-benchmark-sizes=1,4294967293", "--help")
    assert result.returncode == 0, result.stdout

    result = run("--connection-benchmark-sizes=-1,-64,64", "--connection-benchmark-mode=client-only", "--help")
    assert result.returncode == 0, result.stdout

    result = run("--connection-benchmark-backends=mt", "--connection-benchmark-clients=1",
            "--connection-benchmark-sizes=-8", "--benchmark-samples=5",
            "--benchmark-resamples=1000", "benchmark_connection_operations")
    assert result.returncode == 0, result.stdout
    assert all(f"Conn_mt_{mode}_1c_8B_random" in result.stdout for mode in ("send", "receive", "both")), result.stdout
    assert "Random-size MiB/sec uses the expected mean payload size" in result.stdout, result.stdout

    result = run("--connection-benchmark-sizes=4", "--connection-benchmark-mode=client-only", "--help")
    assert result.returncode != 0 and "external mode requires at least 5 bytes" in result.stdout, result.stdout

    result = run("--connection-benchmark-backends=mt",
            "--connection-benchmark-clients=9",
            "--connection-benchmark-clients", "1, 2,1",
            "--connection-benchmark-sizes=256",
            "--connection-benchmark-sizes=1,64,1",
            "--benchmark-samples=5", "--benchmark-resamples=1000",
            "benchmark_connection_operations")
    assert result.returncode == 0, result.stdout
    names = re.findall(r"Conn_mt_(send|receive|both)_(\d+)c_(\d+)B\s", result.stdout)
    expected = {(mode, clients, size) for mode in ("send", "receive", "both")
            for clients in ("1", "2") for size in ("1", "64")}
    assert len(names) == 12 and set(names) == expected, names

    result = run("--connection-benchmark-backends=mt", "--connection-benchmark-clients=1",
            "--connection-benchmark-sizes=64", "--connection-benchmark-window=2",
            "--connection-benchmark-reliability=both", "--benchmark-samples=5",
            "--benchmark-resamples=1000", "benchmark_connection_operations")
    assert result.returncode == 0, result.stdout
    assert "Unreliable results require every packet to arrive" in result.stdout, result.stdout
    for mode in ("send", "receive", "both"):
        for reliability in ("reliable", "unreliable"):
            assert re.search(r"^mt\s+" + mode + r"\s+" + reliability + r"\s+1\s+2\s+64 B\s+", result.stdout, re.M), result.stdout

    result = run("--connection-benchmark-backends=mt", "--connection-benchmark-clients=1",
            "--connection-benchmark-sizes=64", "--connection-benchmark-window=2",
            "--connection-benchmark-reliability=unreliable", "--benchmark-samples=5",
            "--benchmark-resamples=1000", "benchmark_connection_operations")
    assert result.returncode == 0, result.stdout
    for mode in ("send", "receive", "both"):
        assert re.search(r"^mt\s+" + mode + r"\s+unreliable\s+1\s+2\s+64 B\s+", result.stdout, re.M), result.stdout
        assert not re.search(r"^mt\s+" + mode + r"\s+reliable\s+", result.stdout, re.M), result.stdout

    result = run("--connection-benchmark-backends=mt", "--benchmark-samples=5",
            "--benchmark-resamples=1000", "--section", "mt", "--section", "1 clients",
            "--section", "64 B", "benchmark_connection_operations")
    assert result.returncode == 0, result.stdout
    assert all(f"Conn_mt_{mode}_1c_64B" in result.stdout for mode in ("send", "receive", "both")), result.stdout

    # A server-only process handles the whole list, so it must allow the
    # largest requested client count even when the first section is smaller.
    with socket.socket(socket.AF_INET6) as probe:
        probe.bind(("::1", 0))
        port = probe.getsockname()[1]
    server_log = pathlib.Path(directory) / "server.log"
    with server_log.open("w") as output:
        server = subprocess.Popen([str(binary), "--run-benchmarks",
            "--connection-benchmark-backends=mt", "--connection-benchmark-mode=server-only",
            f"--connection-benchmark-port={port}", "--connection-benchmark-clients=1,512",
            "--connection-benchmark-sizes=5", "benchmark_connection_operations"],
            cwd=directory, stdout=output, stderr=subprocess.STDOUT)
        try:
            deadline = time.monotonic() + 10
            while "server ready" not in server_log.read_text():
                assert server.poll() is None and time.monotonic() < deadline, server_log.read_text()
                time.sleep(0.05)
            assert "for 512 mt client(s)" in server_log.read_text(), server_log.read_text()
            result = run("--connection-benchmark-backends=mt",
                    "--connection-benchmark-mode=client-only",
                    f"--connection-benchmark-port={port}", "--connection-benchmark-server=::1",
                    "--connection-benchmark-clients=1", "--connection-benchmark-sizes=5,64,-8",
                    "--connection-benchmark-reliability=both", "--benchmark-samples=5", "--benchmark-resamples=1000",
                    "benchmark_connection_operations")
            assert result.returncode == 0, result.stdout
            names = re.findall(r"Conn_mt_(send|receive|both)_1c_(5|64)B\s", result.stdout)
            assert len(names) == 6 and set(names) == {
                (mode, size) for mode in ("send", "receive", "both") for size in ("5", "64")
            }, names
            for mode in ("send", "receive", "both"):
                for reliability in ("reliable", "unreliable"):
                    rows = re.findall(r"^mt\s+" + mode + r"\s+" + reliability + r"\s+1\s+1\s+(?:5|64) B\s+", result.stdout, re.M)
                    assert len(rows) == 2, result.stdout
            assert all(f"Conn_mt_{mode}_1c_8B_random" in result.stdout for mode in ("send", "receive", "both")), result.stdout
            assert server.poll() is None, server_log.read_text()
        finally:
            if server.poll() is None:
                server.terminate()
                try:
                    server.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    server.kill()
                    server.wait()

print("Connection benchmark option regressions passed")
