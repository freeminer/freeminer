#!/usr/bin/env python3
"""Check production retry policy against the bundled usrsctp socket API.

Pass a compiled libusrsctp.a. No networking or rebuilt game is required.
"""
import pathlib
import subprocess
import sys
import tempfile

root = pathlib.Path(__file__).resolve().parents[4]
header = root / "src/network/sctp/internal.h"
source = header.read_text()
start = source.index("inline void configurePeerTimeout(")
end = source.index("\n}\n", start) + 2
policy = source[start:end]
start = source.index("inline void configureDefaults(")
end = source.index("\n}\n", start) + 2
policy += "\n" + source[start:end]
program = r'''
#include <usrsctp.h>
#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <iostream>
#include <cstring>
inline auto &warningstream = std::cerr;
using ConnectionException = std::runtime_error;
POLICY

int outputs = 0;
int output(void *, void *, size_t, uint8_t, uint8_t) { ++outputs; return 0; }
int main()
{
    usrsctp_init(0, output, nullptr);
    configureDefaults();
    assert(usrsctp_sysctl_get_sctp_blackhole() == 2);
    auto *untimed = usrsctp_socket(AF_CONN, SOCK_STREAM, IPPROTO_SCTP,
                                  nullptr, nullptr, 0, nullptr);
    auto *timed = usrsctp_socket(AF_CONN, SOCK_STREAM, IPPROTO_SCTP,
                                nullptr, nullptr, 0, nullptr);
    assert(untimed && timed);
    sctp_initmsg before{};
    socklen_t length = sizeof(before);
    assert(usrsctp_getsockopt(timed, IPPROTO_SCTP, SCTP_INITMSG, &before, &length) == 0);
    sctp_assocparams defaults{};
    length = sizeof(defaults);
    assert(usrsctp_getsockopt(untimed, IPPROTO_SCTP, SCTP_ASSOCINFO, &defaults, &length) == 0);
    configurePeerTimeout(untimed, false);
    configurePeerTimeout(timed, true);
    for (auto *socket : {untimed, timed}) {
        sctp_assocparams options{};
        length = sizeof(options);
        assert(usrsctp_getsockopt(socket, IPPROTO_SCTP, SCTP_ASSOCINFO, &options, &length) == 0);
        assert(options.sasoc_asocmaxrxt == (socket == timed ? UINT16_MAX : defaults.sasoc_asocmaxrxt));
    }
    sctp_initmsg after{};
    length = sizeof(after);
    assert(usrsctp_getsockopt(timed, IPPROTO_SCTP, SCTP_INITMSG, &after, &length) == 0);
    assert(before.sinit_max_attempts == after.sinit_max_attempts);
    assert(before.sinit_max_init_timeo == after.sinit_max_init_timeo);
    // An unknown-source packet must not elicit the reflected ABORT that
    // terminates a rebinding sender before authenticated ASCONF arrives.
    int address = 0;
    usrsctp_register_address(&address);
    uint8_t packet[16] = {0x23, 0x29, 0x23, 0x2a, 0, 0, 0, 1,
                          0, 0, 0, 0, 4, 0, 0, 4};
    auto checksum = usrsctp_crc32c(packet, sizeof(packet));
    std::memcpy(packet + 8, &checksum, sizeof(checksum));
    assert(usrsctp_sysctl_set_sctp_blackhole(0) == 0);
    usrsctp_conninput(&address, packet, sizeof(packet), 0);
    assert(outputs == 1);
    configureDefaults();
    usrsctp_conninput(&address, packet, sizeof(packet), 0);
    assert(outputs == 1);
    usrsctp_deregister_address(&address);
    usrsctp_close(untimed);
    usrsctp_close(timed);
}
'''.replace("POLICY", policy)
with tempfile.TemporaryDirectory(prefix="sctp-timeout-") as directory:
    path = pathlib.Path(directory)
    (path / "test.cpp").write_text(program)
    subprocess.run([
        "c++", "-std=c++17", "-pthread",
        "-I" + str(root / "src/external/usrsctp/usrsctplib"),
        str(path / "test.cpp"), sys.argv[1], "-o", str(path / "test"),
    ], check=True)
    subprocess.run([str(path / "test")], check=True)
print("PASS: association retry policy, unchanged INIT limits, and no reflected OOTB ABORT")
