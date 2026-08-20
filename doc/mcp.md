# Freeminer MCP integration

When `enable_mcp = true`, Freeminer exposes a standard MCP Streamable HTTP
endpoint at `http://127.0.0.1:3001/mcp`. It supports JSON responses to POST
requests, per-client sessions, notification acknowledgements, session deletion,
and the MCP protocol versions `2025-03-26` and `2025-06-18`. GET returns HTTP
405 because Freeminer currently has no unsolicited server-to-client messages
and does not need an SSE listening stream.

The listener is restricted to IPv4 loopback. Its port can be changed with
`mcp_port`.

### Cline configuration

Cline can connect directly to the Streamable HTTP endpoint. Configure this URL:

```text
http://127.0.0.1:3001/mcp
```

The Freeminer client must already be running with `enable_mcp = true`.

Each HTTP session must perform the MCP `initialize` exchange and send
`notifications/initialized` before listing or calling tools.

## Chat tools

`send_chat_message` sends public chat as the connected player. Its result has a
`delivery` field of `sent` or `queued`; queued messages are sent automatically
when the client chat rate limit permits. A message beginning with `/` is sent as
a server command.

`get_chat_messages` defaults to the structured MCP history. Each message has an
`id`, `type`, `sender`, `text`, `formatted`, and `timestamp`. For polling, pass
the previous response's `next_after_id` as `after_id`. Up to 1,000 messages are
retained and each call returns at most 200.
