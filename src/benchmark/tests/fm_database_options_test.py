#!/usr/bin/env python3
"""Exercise database benchmark block-size options against a built binary."""
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile

source = pathlib.Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix="fm-database-benchmark-") as directory:
    root = pathlib.Path(directory)
    binary = root / "bin" / source.name
    binary.parent.mkdir()
    shutil.copy2(source, binary)

    def run(*args):
        return subprocess.run([str(binary), "--run-benchmarks", *args], cwd=root,
                text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=90)

    for value in ("", "0", "-0", "-4294967296", "1,,2", "64,", "1KiB", "4294967296"):
        result = run("--database-benchmark-sizes=" + value, "--help")
        assert result.returncode != 0 and "Invalid --database-benchmark-sizes" in result.stdout, result.stdout
    result = run("--database-benchmark-sizes")
    assert result.returncode != 0 and "requires a value" in result.stdout, result.stdout
    for options, expected in (([], {256, 1024, 4096, 16384, 65536}),
            (["--database-benchmark-sizes=9", "--database-benchmark-sizes", "7, 13,7"], {7, 13})):
        result = run(*options, "--benchmark-samples=1", "--benchmark-resamples=1",
                "--benchmark-warmup-time=1", "--benchmark-no-analysis",
                "--section", "SaveBlock Operations", "benchmark_database_operations")
        assert result.returncode == 0, result.stdout
        sizes = re.findall(r"^DBWrite_Dummy_(\d+)B\s", result.stdout, re.M)
        assert len(sizes) == len(expected) and {int(size) for size in sizes} == expected, result.stdout
    result = run("--database-benchmark-sizes=7,13", "--benchmark-samples=1",
            "--benchmark-resamples=1", "--benchmark-warmup-time=1",
            "--benchmark-no-analysis", "--section", "LoadBlock Operations",
            "benchmark_database_operations")
    assert result.returncode == 0, result.stdout
    sizes = re.findall(r"^DBRead_Dummy_(\d+)B\s", result.stdout, re.M)
    assert len(sizes) == 2 and set(sizes) == {"7", "13"}, result.stdout
    result = run("test_database_benchmark_random_sizes")
    assert result.returncode == 0 and "All tests passed" in result.stdout, result.stdout
    for section, prefix in (("SaveBlock Operations", "DBRandomWrite"),
                            ("LoadBlock Operations", "DBRandomRead")):
        result = run("--database-benchmark-sizes=-1,-16,7,-16", "--benchmark-samples=1",
                "--benchmark-resamples=1", "--benchmark-warmup-time=1",
                "--benchmark-no-analysis", "--section", section,
                "benchmark_database_operations")
        assert result.returncode == 0, result.stdout
        sizes = re.findall(r"^" + prefix + r"_Dummy_(\d+)B\s", result.stdout, re.M)
        assert len(sizes) == 2 and set(sizes) == {"1", "16"}, result.stdout
        assert re.search(r"^DB(?:Write|Read)_Dummy_7B\s", result.stdout, re.M), result.stdout
print("Database benchmark option regressions passed")
