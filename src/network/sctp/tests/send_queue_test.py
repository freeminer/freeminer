#!/usr/bin/env python3
"""Exercise the production SCTP send loop against deterministic socket backpressure.

Run with python3 send_queue_test.py; requires a C++17 compiler.
The fake socket API lets failures and short writes be tested without a network.
"""
import pathlib
import subprocess
import tempfile

SOURCE = pathlib.Path(__file__).resolve().parents[1] / "send.cpp"
MOCK = r'''
#pragma once
#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cstring>
#include <cstdint>
#include <deque>
#include <memory>
#include <iostream>
inline auto &warningstream = std::cerr;
#include <unordered_map>
#include <unordered_set>
#include <vector>
#define USE_SCTP 1
#define CHANNEL_COUNT 3
using u8 = uint8_t;
using session_t = unsigned;
constexpr unsigned SCTP_SEND_SNDINFO_VALID = 1, SCTP_SEND_PRINFO_VALID = 2;
constexpr unsigned SCTP_UNORDERED = 4, SCTP_EOR = 8;
constexpr unsigned SCTP_PR_SCTP_RTX = 3, SCTP_SENDV_SPA = 1;
struct socket { unsigned id; };
struct sctp_sendv_spa {
 unsigned sendv_flags = 0;
 struct { unsigned snd_sid = 0, snd_flags = 0; } sendv_sndinfo;
 struct { unsigned pr_policy = 0, pr_value = 0; } sendv_prinfo;
};
template<class T> struct SharedBuffer {
 std::shared_ptr<std::vector<T>> bytes;
 explicit SharedBuffer(size_t n) : bytes(std::make_shared<std::vector<T>>(n)) {}
 size_t getSize() const { return bytes->size(); }
 T *operator*() const { return bytes->data(); }
};
struct PeerMap : std::unordered_map<session_t, socket *> {
 int lock_unique_rec() { return 0; }
};
namespace con_sctp {
inline void logPaths(session_t, socket *, const char *) {}
struct Connection {
 struct PendingSend { u8 channel; SharedBuffer<u8> data; bool reliable; size_t offset=0; };
 PeerMap m_peers;
 std::unordered_set<session_t> m_established_peers;
 std::unordered_map<session_t, std::deque<PendingSend>> pending_sends;
 socket *getPeer(session_t id) { auto i=m_peers.find(id); return i==m_peers.end()?nullptr:i->second; }
 bool deletePeer(session_t id, bool) { m_peers.erase(id); m_established_peers.erase(id); pending_sends.erase(id); return true; }
 void sendToAll(u8, SharedBuffer<u8>, bool);
 void send_(session_t, u8, SharedBuffer<u8>, bool);
 void flushPendingSends();
};
}
struct Write { unsigned peer; size_t size; sctp_sendv_spa info; };
inline std::vector<Write> writes;
inline std::unordered_map<unsigned, int> failures;
inline bool short_write = false;
inline int usrsctp_sendv(socket *s, const void *, size_t n, void *, int,
 void *info, size_t, unsigned, unsigned flags) {
 assert(flags == 0);
 writes.push_back({s->id,n,*static_cast<sctp_sendv_spa *>(info)});
 if (failures[s->id]) { errno=failures[s->id]; return -1; }
 if (short_write) { short_write=false; return static_cast<int>(n/2); }
 return static_cast<int>(n);
}
'''
# Use the production error classification, just like the production send loop.
internal = (SOURCE.parent / "internal.h").read_text()
retry_helper = internal.split("inline bool isRetryableError", 1)[1].split(
    "inline void configureDefaults", 1
)[0]
MOCK = MOCK.replace("namespace con_sctp {", "namespace con_sctp {\ninline bool isRetryableError" + retry_helper, 1)

TEST = r'''
int main() {
 using con_sctp::Connection;
 socket a{1}, b{2};
 Connection c;
 c.m_peers[1]=&a; c.m_peers[2]=&b;
 c.send_(1, 0, SharedBuffer<u8>(10), true);
 c.flushPendingSends();
 assert(writes.empty()); // handshake pending: no application data reaches SCTP
 assert(c.pending_sends[1].size()==1);
 c.m_established_peers.insert(1); c.m_established_peers.insert(2);
 c.flushPendingSends();
 assert(c.pending_sends[1].empty());
 writes.clear();
 c.send_(1, 0, SharedBuffer<u8>(100), true);
 c.send_(2, 1, SharedBuffer<u8>(200), true);
 failures[1]=EWOULDBLOCK;
 c.flushPendingSends();
 assert(c.pending_sends[1].size()==1);
 assert(c.pending_sends[1].front().offset==0);
 assert(c.pending_sends[2].empty()); // congested peer did not block another
 for (int error : {ENETUNREACH, EHOSTUNREACH, ENETDOWN, EADDRNOTAVAIL, ENOBUFS}) {
  failures[1]=error;
  c.flushPendingSends();
  assert(c.getPeer(1)); // allow SCTP to recover during address handoff
  assert(c.pending_sends[1].front().offset==0);
 }
 failures[1]=0;
 errno=EWOULDBLOCK; // stale errno must not affect a successful write
 short_write=true;
 c.flushPendingSends();
 assert(c.pending_sends[1].empty());
 assert(writes.back().size==50); // retry only the remaining bytes
 writes.clear();
 c.send_(1, 2, SharedBuffer<u8>(100000), true);
 c.flushPendingSends();
 assert(writes.size()==2);
 assert(!(writes[0].info.sendv_sndinfo.snd_flags&SCTP_EOR));
 assert(writes[1].info.sendv_sndinfo.snd_flags&SCTP_EOR);
 assert(writes[0].size+writes[1].size==100000);
 c.send_(2, 2, SharedBuffer<u8>(32), false);
 c.flushPendingSends();
 auto spa=writes.back().info;
 assert(spa.sendv_sndinfo.snd_sid==7);
 assert(spa.sendv_sndinfo.snd_flags&SCTP_UNORDERED);
 assert(spa.sendv_prinfo.pr_policy==SCTP_PR_SCTP_RTX);
 assert(spa.sendv_prinfo.pr_value==0);
 c.send_(1,0,SharedBuffer<u8>(10),true);
 failures[1]=EPIPE;
 c.flushPendingSends();
 assert(!c.getPeer(1) && !c.pending_sends.count(1));
 for (unsigned i=0;i<20;++i) c.send_(2,0,SharedBuffer<u8>(10),true);
 c.flushPendingSends();
 assert(c.pending_sends[2].size()==4); // bounded work per peer
 c.flushPendingSends();
 assert(c.pending_sends[2].empty());
 c.send_(2,0,SharedBuffer<u8>(10),true);
 c.m_peers.erase(2); // derived transports can remove sockets directly
 c.flushPendingSends();
 assert(!c.pending_sends.count(2));
}
'''
with tempfile.TemporaryDirectory(prefix="fm-sctp-test-") as tmp:
    root = pathlib.Path(tmp)
    header = root / "network/sctp/internal.h"
    header.parent.mkdir(parents=True)
    header.write_text(MOCK)
    source = root / "test.cpp"
    source.write_text(f'#include "{SOURCE}"\n' + TEST)
    binary = root / "test"
    subprocess.run(["c++", "-std=c++17", "-I", tmp, str(source), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
print("SCTP send queue regression checks passed")
