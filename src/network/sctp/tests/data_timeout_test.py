#!/usr/bin/env python3
"""Run the production peer watchdog with control-only traffic and a fake clock."""
import pathlib
import subprocess
import tempfile

source = (pathlib.Path(__file__).resolve().parents[1] / "impl.cpp").read_text()
start = source.index("void Connection::checkPeerTimeouts()")
end = source.index("\n}\n", start) + 2
program = r'''
#include <cstdint>
#include <vector>
#include <utility>
#include <unordered_map>
#include <iostream>
#include <cassert>
using u32 = uint32_t;
using session_t = unsigned;
using socklen_t = unsigned;
constexpr unsigned PEER_ID_SERVER = 1, IPPROTO_SCTP = 132, SCTP_PEER_ACTIVITY = 1;
u32 clock_ms = 0, sequence = 0;
bool query_ok = true;
namespace porting { u32 getTimeMs() { return clock_ms; } }
struct sctp_assoc_value { u32 assoc_value = 0; };
int usrsctp_getsockopt(void *, int, int, sctp_assoc_value *v, socklen_t *) {
 v->assoc_value = sequence; return query_ok ? 0 : -1;
}
auto &warningstream = std::cerr;
void logPaths(session_t, void *, const char *) {}
struct Connection {
 u32 m_peer_timeout_ms = 30000, m_last_timeout_check = 0, m_last_recieved = 0;
 std::unordered_map<session_t, std::pair<u32, u32>> m_peer_activity;
 std::vector<session_t> deleted;
 void *getPeer(session_t) { return this; }
 void deletePeer(session_t id, bool timeout) {
  assert(timeout); deleted.push_back(id); m_peer_activity.erase(id);
 }
 void checkPeerTimeouts();
};
FUNCTION
int main() {
 // Continuous control traffic must not preserve a client without game data.
 Connection client; client.m_peer_activity[1] = {0, 0};
 for (clock_ms = 1000; clock_ms <= 30000; clock_ms += 1000) {
  ++sequence; client.checkPeerTimeouts();
 }
 assert(client.deleted == std::vector<session_t>{1});
 // Periodic game data keeps the client healthy beyond the deadline.
 Connection healthy; healthy.m_peer_activity[1] = {sequence, clock_ms};
 healthy.m_last_recieved = clock_ms;
 for (unsigned i = 0; i < 100; ++i) {
  clock_ms += 1000; ++sequence;
  if (i % 5 == 0) healthy.m_last_recieved = clock_ms;
  healthy.checkPeerTimeouts();
 }
 assert(healthy.deleted.empty());
 // Server-side idle peers can still use heartbeat/control liveness.
 Connection server; server.m_peer_activity[100] = {sequence, clock_ms};
 for (unsigned i = 0; i < 40; ++i) {
  clock_ms += 1000; ++sequence; server.checkPeerTimeouts();
 }
 assert(server.deleted.empty());
 // Query failures must not refresh a deadline.
 query_ok = false; clock_ms += 30000; server.checkPeerTimeouts();
 assert(server.deleted == std::vector<session_t>{100});
 // Zero retains the documented opt-out.
 Connection disabled; disabled.m_peer_timeout_ms = 0;
 disabled.m_peer_activity[1] = {0, 0}; disabled.checkPeerTimeouts();
 assert(disabled.deleted.empty());
 // Monotonic millisecond wrap must preserve the interval.
 Connection wrapped; wrapped.m_last_timeout_check = UINT32_MAX - 999;
 wrapped.m_last_recieved = UINT32_MAX - 999;
 wrapped.m_peer_activity[1] = {sequence, UINT32_MAX - 999};
 clock_ms = 29000; wrapped.checkPeerTimeouts();
 assert(wrapped.deleted == std::vector<session_t>{1});
 std::cout << "PASS: control-only client timeout, game progress, idle server, query failure, disable and wrap\n";
}
'''.replace('FUNCTION', source[start:end])
with tempfile.TemporaryDirectory(prefix="sctp-data-timeout-") as directory:
    path = pathlib.Path(directory)
    (path / "test.cpp").write_text(program)
    subprocess.run(["c++", "-std=c++17", "-Wall", "-Wextra", "-Werror", str(path / "test.cpp"), "-o", str(path / "test")], check=True)
    subprocess.run([str(path / "test")], check=True)
