# SCTP over WebSocket

`ws_sctp` carries complete usrsctp output packets as binary WebSocket messages.
Received messages go directly to `usrsctp_conninput()` using one opaque `AF_CONN`
address per WebSocket. SCTP still provides streams, message boundaries, and
association events. This is a different wire protocol from `ws`'s emsocket
`NEWADDR`/`BIND` datagram service.

The shared `network/ws` implementation owns endpoint setup, HTTP serving, TLS
contexts, URI handling, binary message queues, and connection lifecycle. Native
builds use WebSocket++; Emscripten uses `emscripten/websocket.h` directly, without
emulated TCP or a WebSocket-to-UDP proxy. Incoming browser callbacks run on the
main runtime thread and queue copies for the connection worker. SCTP output callbacks
queue packets; they never drive a WebSocket endpoint from usrsctp timer threads.
Both queues are bounded to 16 MiB.

For an Emscripten client, configure with `-DUSE_MULTI=OFF
-DENABLE_WEBSOCKET=OFF -DENABLE_WEBSOCKET_SCTP=ON`. The build links
`websocket.js`, and builds the bundled usrsctp with native IPv4/IPv6 disabled;
`AF_CONN` is the carrier. Existing project pthread support is still required.
Browser listeners are unsupported.

For a native server, enable `ENABLE_WEBSOCKET_SCTP` and select the advertised
`ws_sctp` port. Plain WebSockets remain the default. Set
`ENABLE_WEBSOCKET_SCTP_TLS=ON` to use TLS with the existing `https_chain` and
`https_key` certificate settings, or put the plain listener behind a TLS reverse
proxy. The ordinary `ws` listener retains its existing TLS policy.

The browser selects `wss://` on HTTPS pages and `ws://` on HTTP pages. An optional
`ws_sctp_url`, for example `wss://game.example.org/sctp`, overrides the URL when a
DNS certificate name or reverse-proxy path is needed. The selected game address
and port still determine the SCTP association port; the proxy must route to that
server's `ws_sctp` listener. Ordinary `ws` clients cannot connect to this listener.
