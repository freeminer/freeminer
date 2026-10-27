#!/usr/bin/env python3
"""Release an upcall reference during production usrsctp socket teardown.

Usage: python3 fm_socket_lifetime_test.py [CMake build directory]
Requires a built bundled usrsctp library and compile_commands.json (Linux).
"""
import json
import pathlib
import shlex
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[4]
BUILD = pathlib.Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else ROOT / "build_-20"
SOURCE = r'''
#include <netinet/sctp_os.h>
#include <netinet/sctp_pcb.h>
#include <netinet/sctputil.h>
#include <netinet/sctp_var.h>
#include <assert.h>
extern void usrsctp_init(uint16_t,
        int (*)(void *, void *, size_t, uint8_t, uint8_t),
        void (*)(const char *, ...));
extern struct socket *usrsctp_socket(int, int, int,
        int (*)(struct socket *, union sctp_sockstore, void *, size_t,
                struct sctp_rcvinfo, int, void *),
        int (*)(struct socket *, uint32_t, void *), uint32_t, void *);
extern void usrsctp_close(struct socket *);
extern int usrsctp_finish(void);
extern void __real_sctp_close(struct socket *);
static unsigned detaches;
void __wrap_sctp_close(struct socket *so)
{
    assert(++detaches == 1);
    /* An input-thread upcall temporarily references the socket after
     * protocol detach has dropped the socket lock. */
    SOCK_LOCK(so);
    soref(so);
    SOCK_UNLOCK(so);
    ACCEPT_LOCK();
    SOCK_LOCK(so);
    sorele(so);
    __real_sctp_close(so);
}
int main(void)
{
    usrsctp_init(0, NULL, NULL);
    struct socket *so = usrsctp_socket(AF_INET6, SOCK_STREAM, IPPROTO_SCTP,
            NULL, NULL, 0, NULL);
    assert(so);
    usrsctp_close(so);
    assert(detaches == 1);
    for (unsigned i = 0; i < 1000; ++i) {
        if (usrsctp_finish() == 0)
            return 0;
        usleep(1000);
    }
    return 1;
}
'''

entries = json.loads((BUILD / "compile_commands.json").read_text())
entry = next(e for e in entries if e["file"].endswith("usrsctplib/user_socket.c"))
command = shlex.split(entry["command"])
# Reuse the library's configuration and headers; keep assertions enabled.
command = command[:command.index("-o")]
with tempfile.TemporaryDirectory(prefix="fm-sctp-lifetime-") as directory:
    source = pathlib.Path(directory) / "test.c"
    binary = pathlib.Path(directory) / "test"
    source.write_text(SOURCE)
    subprocess.run(command + ["-UNDEBUG", str(source),
        str(BUILD / "src/external/usrsctp/usrsctplib/libusrsctp.a"),
        "-Wl,--wrap=sctp_close", "-pthread", "-o", str(binary)],
        cwd=entry["directory"], check=True)
    subprocess.run([str(binary)], check=True, timeout=10)
print("SCTP socket detach lifetime regression passed")
