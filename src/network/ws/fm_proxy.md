# WebSocket DNS and TCP proxy

The server accepts emsocket's text handshake on its existing WebSocket endpoint:

- `PROXY IPV6 TCP fd00::1 53`: DNS resolver, available by default.
- `PROXY IPV6 TCP fd00::1 8080` (or `3128`): HTTP CONNECT proxy.
- `PROXY IPV4 TCP <IPv4> <port>` or `PROXY IPV6 TCP <IPv6> <port>`:
  outbound TCP connection.

Set `ws_proxy_enable = true` in the server configuration to enable outbound TCP
and HTTP CONNECT. This permits WebSocket clients to reach addresses accessible
from the server, including its local network. There is no proxy authentication;
restrict access to the WebSocket endpoint when enabling this setting.

Successful handshakes return the text frame `PROXY OK`; failures return
`PROXY FAILED` and close. Direct TCP is acknowledged only after connecting.
Subsequent tunnel frames are binary. HTTP CONNECT accepts a hostname or a
bracketed IPv6 address with a port and returns HTTP 200 after connecting, 400
for malformed requests, 431 for oversized headers, or 502 on upstream failure.

DNS queries occupy one WebSocket message: `<hostname> A`, `<hostname> AAAA`, or
`<hostname> ANY`. Replies contain a one-byte count followed by records: a byte
with value 4 or 6, then 4 or 16 address bytes in network order. Lookup failure
returns a zero count. Queries must be sequential on a connection. Resolution
and connection setup time out after 30 seconds; pending tunnel buffers are
limited to 1 MiB per direction. Slow consumers exceeding this limit disconnect.

Game requests for this listener's address and port go directly to the game
packet queue. The server also resolves `server_address` at startup and recognizes
its IPv4/IPv6 addresses, including a public IPv4 address behind NAT. Configure
`server_address` to the hostname players use when it differs from the listener's
local address. The legacy synthetic address `10.0.0.1` is also accepted on the
game port. These game routes do not require `ws_proxy_enable`.
Private-network address assignment (`NEWADDR`) and encapsulated UDP (`BIND`)
are separate protocols and are not implemented by this service.

The WASM client can use real DNS results for the game destination. In direct
game mode its shim labels game traffic `PROXY ... TCP` even though each following WebSocket
message contains one game datagram. Outbound TCP proxying applies to destinations
that do not match this server's game route.

Newer emsocket clients first send `NEWADDR` and expect `ADDR <address> <passcode>
<joincode>`, then use `BIND` and encapsulated datagrams. This endpoint's legacy
first-message acknowledgement is `PROXY OK`, including for `NEWADDR`. The shim
must recognize that reply as direct game mode and open a fresh `PROXY ... TCP`
connection on its first outgoing datagram. Simply accepting `PROXY OK` as an
address assignment will not work: this endpoint does not implement `BIND` or
the 24-byte UDP envelope. Proxies returning `ADDR ...` keep using those protocols.

## Protocol tests

Build `tests/fm_proxy_server.cpp` as a standalone executable with the same Boost
and WebSocket++ include paths as the server, C++20, and pthread support. Run:

```
python3 src/network/ws/tests/fm_proxy_test.py /path/to/fm_proxy_server
```

The tests require Python's `websockets` package and use local DNS and loopback
TCP listeners. They cover IPv4/IPv6 records, invalid input, disabled proxying,
resolved game addresses (including NAT and IPv4-mapped IPv6), TCP streaming, fragmented CONNECT headers with initial tunnel data, refused
connections, and disconnects during resolution.
