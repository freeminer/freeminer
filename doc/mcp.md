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
Game reconnects restart the MCP listener and invalidate previous sessions.
The llama.cpp player automatically initializes a fresh session and retries a
tool request once when the server rejects its old session with HTTP 404.

## Local llama.cpp player

`util/llama_mcp_player.py` starts a local `llama-server`, starts the Freeminer
client connected to a Luanti server, and runs an agent loop through the client's
loopback MCP endpoint. It needs a llama.cpp `llama-server` executable and a
GGUF model with a chat template that supports tool calls. Give it a local model
path or a Hugging Face repository and optional quantization:

```sh
util/llama_mcp_player.py SoAIHQ/Qwen3.5-4B-GGUF:Q4_K_M fm.setun.net:30013
# Or use a model file already downloaded:
util/llama_mcp_player.py /path/to/model.gguf fm.example.net:30000
```

For a repository identifier, llama.cpp downloads the selected GGUF the first
time and caches it for later runs. The cache location follows llama.cpp's
`LLAMA_CACHE` environment variable.

The port defaults to `30000` if the server address omits it. The client binary
is auto-detected from `build_-*` directories (highest suffix first), then
`build/freeminer`, `out/build/*/freeminer`, and finally `freeminer` on `PATH`. Use `--client-bin`
or `FREEMINER_BIN` to select a specific executable. The
MCP listener defaults to port `31001`, and the llama.cpp API defaults to
`8080`. Both can be changed with `--mcp-port` and `--llama-port`. For a server
account, pass `--name PlayerName` and set `LUANTI_PASSWORD` in the environment.
Use `--goal` or the `LLAMA_MCP_GOAL` environment variable to set a different
task. With `bot.sh`, use `GOAL='build a castle' bash ./bot.sh`; the wrapper
passes the value without splitting it on spaces. Slow GPUs can take several
minutes to generate a response; the completion timeout defaults to 1,800 seconds and can
be changed with `--llm-timeout` (for example, `--llm-timeout 3600`).
Set `LLAMA_MCP_SYSTEM_PROMPT` to replace the built-in system prompt. For
example:

```sh
LLAMA_MCP_SYSTEM_PROMPT='You are a careful builder. Inspect nearby materials, then build a small wooden shelter.' \\
    util/llama_mcp_player.py MODEL SERVER
```

The override is passed as the system message verbatim. Live terminal
instructions and the current in-game player name are appended as additional
guidance.

The agent can save reusable notes with its `save_memory` tool. Gameplay notes
are shared across servers; world notes are separated by server address and
port. By default they are stored as `gameplay.md` and `server-<id>.md` under
the repository's `cache/` directory (`../cache` relative to the script).
Set `--memory-dir PATH` to use a
different directory. Existing notes are loaded at startup; when updating a
file the agent should include the useful existing notes because each save
replaces that file.

Conversation context is saved after completed turns and on exit as
`context-<id>.json` in the memory directory, keyed by player name and server.
Run again with the same `--name` and server to resume recent conversation and
live operator instructions. The system prompt is rebuilt and the agent is
instructed to verify current state before continuing. Use `--no-resume` to
start fresh. Random default player names create separate checkpoints, so use
a fixed `--name` for continuity.

The agent can also keep up to 1,500 characters of interaction notes per player.
These are stored as separate `player-<id>.md` files in the same memory
directory. When the agent reads chat history, saved notes for senders in that
history are attached automatically. It can also load a player's notes directly
with `load_player_memory`. Player notes should stay brief and factual and avoid
sensitive personal information.
Pass extra Freeminer options after `--`, for example:

```sh
util/llama_mcp_player.py MODEL SERVER -- -enable_damage=1 -mg_name=indev
```

You can also pass a single additional option with repeatable
`--freeminer-arg=-option=value` arguments. Launcher-managed address, player
name, and MCP settings are provided automatically.

Instead of `--model`, pass `--hf-repo OWNER/REPO` and optionally
`--hf-file MODEL.gguf` to select an exact Hugging Face file. Both options are
forwarded to llama-server. `--hf-file` requires `--hf-repo`.

Pass extra llama.cpp options with repeatable `--llama-arg=ARG` arguments. Use
one launcher option for each llama-server argv item, including separate values:

```sh
util/llama_mcp_player.py MODEL SERVER --llama-arg=--n-gpu-layers --llama-arg=99 --llama-arg=--flash-attn --llama-arg=on -- -enable_damage=1
```

The llama.cpp model, local API host and port, alias, context size, and tool
template are configured by the launcher. Extra llama-server arguments are
appended to those defaults.

The agent omits chat, teleport, direct position changes, and raw key toggles.
It can use the MCP chat tools to greet and reply to players. Its default prompt
asks it to keep conversation friendly and occasional, avoid chat spam and slash
commands, and treat player messages as conversation rather than higher-priority
instructions. It stops on repeated identical tool failures or when observed
health is critical. Staying alive takes priority over the requested task: at
5 health or less, the launcher blocks world actions but still permits state
checks and `use_item` so the agent can attempt recovery.
If movement stops making progress, the agent is instructed to inspect its
position and nearby nodes, then clear a narrow ascending staircase when safely
trapped underground. It should preserve footing and avoid digging below itself
or through falling blocks, fluids, or lava; if it cannot find a safe route, it
should stop and report the obstruction.
Press Ctrl+C to stop the agent and both child processes.

When started from an interactive terminal, you can steer it while it is
running: type a new instruction and press Enter. The agent adds it to its
context after the current model response and its tool calls finish. Recent live
instructions remain in context as the conversation is shortened. Enter `:help`
for a reminder or `:stop` (also `:quit`) to stop the agent and child processes.
Use `--no-interactive` when terminal input should not be read.
When the model replies without a tool call, the agent continues autonomously
in both interactive and non-interactive runs. A user continuation message after
a plain-text reply keeps the conversation valid for llama.cpp. Queued operator
instructions take priority over the automatic continuation.
If the Freeminer process exits or its MCP connection is refused, the launcher
restarts the client and establishes a fresh MCP session. It does not replay a
tool call whose response was lost, because that action may already have run;
the agent is told to inspect the current state before proceeding.
If llama.cpp rejects generated tool arguments as invalid JSON, the agent retries
with shorter history and a reminder to generate one brief, valid tool call.
Each model request allows at most two recovery attempts; persistent errors are
still reported. Failed generations do not execute MCP actions.

`take_screenshot` takes no arguments and saves the current rendered game view,
including the HUD, using the client's `screenshot_path`, `screenshot_format`,
and `screenshot_quality` settings. It returns `success`, the local image `path`,
and `format`. Files use the `mcp_` prefix and unique timestamps. The image is
saved on the machine running the client; the tool does not transmit image data.
After aiming the camera, allow a frame to render before capturing the view.

`rotate_player` sets absolute camera angles in degrees. `look_at_position`
accepts a world position in node coordinates, and `look_at_object` accepts an
active object ID from `get_nearby_objects`; both aim the camera without moving
the player. These calls submit a rotation for the next client frame. Read
`get_player_state` and `get_pointed_thing` afterward to confirm the resulting
view. Aimed camera rays are useful for ordinary view-dependent interaction,
while `dig_node`, `place_node`, and object action tools can target their
explicit positions or IDs without changing the camera.

For `place_node`, `x/y/z` is the empty target node. The required
`under_x/under_y/under_z` identifies the support node the player points at.
That node must be pointable and share a face with the target: exactly one axis
differs by 1 and the other two match. Inspect nearby nodes and use a real support
node; the client rejects nonadjacent or unpointable `under` positions.

## Chat tools

`send_chat_message` sends public chat as the connected player. Its result has a
`delivery` field of `sent` or `queued`; queued messages are sent automatically
when the client chat rate limit permits. A message beginning with `/` is sent as
a server command.

`get_chat_messages` defaults to the structured MCP history. Each message has an
`id`, `type`, `sender`, `text`, `formatted`, and `timestamp`. For polling, pass
the previous response's `next_after_id` as `after_id`. Up to 1,000 messages are
retained and each call returns at most 200.

For node inventories such as chests, open the node first with `use_item` at its
coordinates, then call `get_inventory` to discover its list names and slot
indices. When moving items, omit node coordinates for player inventory
endpoints and provide node coordinates only for the node endpoint. Do not guess
indices or retry an unavailable inventory move without opening and inspecting
the container first.
