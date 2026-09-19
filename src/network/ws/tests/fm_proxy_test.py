#!/usr/bin/env python3
"""Protocol integration tests. Pass the compiled fm_proxy_server executable."""
import asyncio
from contextlib import asynccontextmanager
import ipaddress
import subprocess
import struct
import sys

import websockets
from websockets.legacy.client import connect


@asynccontextmanager
async def connected(ws):
    try:
        yield ws
    finally:
        await ws.close()


async def run(binary, enabled):
    process = subprocess.Popen(
        [binary, "enabled" if enabled else "disabled"], stdout=subprocess.PIPE, text=True
    )
    try:
        port = int(process.stdout.readline())
        uri = f"ws://127.0.0.1:{port}"

        async def open_service(request, expected="PROXY OK"):
            ws = await connect(uri)
            await ws.send(request)
            assert await ws.recv() == expected
            return ws

        async def closed(ws):
            try:
                await ws.recv()
                raise AssertionError("Expected closed connection")
            except websockets.exceptions.ConnectionClosed:
                pass

        async def dns(host, kind):
            ws = await open_service("PROXY IPV6 TCP fd00::1 53")
            async with connected(ws):
                await ws.send(f"{host} {kind}".encode())
                reply = await ws.recv()
                assert isinstance(reply, bytes)
                offset = 1
                addresses = []
                for _ in range(reply[0]):
                    tag = reply[offset]
                    assert tag in (4, 6)
                    size = 4 if tag == 4 else 16
                    addresses.append(ipaddress.ip_address(reply[offset + 1:offset + 1 + size]))
                    offset += 1 + size
                assert offset == len(reply)
                return addresses

        assert await dns("127.0.0.1", "A") == [ipaddress.ip_address("127.0.0.1")]
        assert await dns("::1", "AAAA") == [ipaddress.ip_address("::1")]
        assert ipaddress.ip_address("127.0.0.1") in await dns("localhost", "ANY")
        assert await dns("127.0.0.1", "AAAA") == []
        assert await dns("localhost", "MX") == []
        assert await dns("localhost A", "extra") == []
        assert await dns("a" * 254, "A") == []

        for request in (
            "PROXY IPV4 TCP ::1 53", "PROXY IPV6 TCP fd00::1 0",
            "PROXY IPV6 TCP fd00::1 65536", "PROXY IPV6 TCP fd00::1 53 junk",
            "PROXY IPV6 TCP fd00::1 -1", "PROXY IPV6 UDP fd00::1 53",
            "PROXY IPV6 TCP fd00::1 54", "PROXY IPV6 TCP localhost 53",
        ):
            ws = await open_service(request, "PROXY FAILED")
            await closed(ws)

        def envelope(host, port, payload):
            address = ipaddress.ip_address(host)
            if address.version == 4:
                address = ipaddress.ip_address("::ffff:" + host)
            return struct.pack("!I16sHH", 0x778B4CF6, address.packed, port, len(payload)) + payload

        async def assignment():
            async with connected(await connect(uri)) as ws:
                await ws.send("NEWADDR")
                command, address, passcode, joincode = (await ws.recv()).split(" ")
                assert command == "ADDR" and ipaddress.ip_address(address).version == 6
                assert address != "fd00::1"
                assert len(passcode) == 32 and len(joincode) == 16
                assert passcode != joincode
                return address, passcode

        address, passcode = await assignment()
        address2, passcode2 = await assignment()
        assert address != address2 and passcode != passcode2
        game = await open_service(f"BIND {passcode} UDP 12345", "BIND OK")
        async with connected(game):
            # Check game routes, NAT, IPv4-mapped IPv6 and unchanged binary payloads.
            for host in ("127.0.0.1", "::ffff:127.0.0.1", "192.0.2.10", "10.0.0.1"):
                packet = envelope(host, port, bytes(range(256)))
                await game.send(packet)
                assert await game.recv() == packet

            duplicate = await open_service(f"BIND {passcode} UDP 12345", "BIND FAILED")
            async with connected(duplicate):
                # A failed bind leaves the connection open for a fresh assignment.
                await duplicate.send(f"BIND {passcode2} UDP 12345")
                assert await duplicate.recv() == "BIND OK"
                await duplicate.send(envelope(address, 12345, b"peer packet"))
                assert await game.recv() == envelope(address2, 12345, b"peer packet")
                await game.send(envelope(address2, 12345, b"reply"))
                assert await duplicate.recv() == envelope(address, 12345, b"reply")

            other_port = await open_service(f"BIND {passcode} UDP 12346", "BIND OK")
            async with connected(other_port):
                await other_port.send(envelope(address, 12345, b"same address"))
                assert await game.recv() == envelope(address, 12346, b"same address")

            # Unknown destinations must never enter the game queue.
            await game.send(envelope("192.0.2.99", port, b"wrong host"))
            await game.send(envelope("127.0.0.1", (port % 65535) + 1, b"wrong port"))
            good = envelope("127.0.0.1", port, b"after dropped packets")
            await game.send(good)
            assert await game.recv() == good

        # Closing a connection releases its port but retains its assignment.
        rebound = await open_service(f"BIND {passcode} UDP 12345", "BIND OK")
        await rebound.close()
        for request in (
            "BIND missing UDP 12345", f"BIND {passcode} TCP 12345",
            f"BIND {passcode} UDP 0", f"BIND {passcode} UDP 65536",
            f"BIND {passcode} UDP -1", f"BIND {passcode} UDP 12345 extra",
            "BIND", "BIND " + "x" * 200,
        ):
            failed = await open_service(request, "BIND FAILED")
            async with connected(failed):
                await failed.send(f"BIND {passcode} UDP 12347")
                assert await failed.recv() == "BIND OK"

        good = envelope("127.0.0.1", port, b"payload")
        for packet in (b"short", b"xxxx" + good[4:], good[:-1], good + b"extra",
                       envelope("127.0.0.1", 0, b"invalid port"), "NEWADDR"):
            ws = await open_service(f"BIND {passcode} UDP 12348", "BIND OK")
            await ws.send(packet)
            await closed(ws)
        # Data without an authenticated bind is rejected.
        async with connected(await connect(uri)) as ws:
            await ws.send(good)
            await closed(ws)

        if not enabled:
            for target in ("IPV4 TCP 127.0.0.1 80", "IPV6 TCP fd00::1 8080"):
                ws = await open_service("PROXY " + target, "PROXY FAILED")
                await closed(ws)
            return

        async def echo(reader, writer):
            try:
                while data := await reader.read(16384):
                    writer.write(data)
                    await writer.drain()
            finally:
                writer.close()
                await writer.wait_closed()

        echo_server = await asyncio.start_server(echo, "127.0.0.1", 0)
        echo_port = echo_server.sockets[0].getsockname()[1]
        async with echo_server:
            ws = await open_service(f"PROXY IPV4 TCP 127.0.0.1 {echo_port}")
            async with connected(ws):
                payload = bytes(range(256)) * 1024
                for offset in range(0, len(payload), 7000):
                    await ws.send(payload[offset:offset + 7000])
                received = b""
                while len(received) < len(payload):
                    received += await ws.recv()
                assert received == payload

            for service_port in (8080, 3128):
                ws = await open_service(f"PROXY IPV6 TCP fd00::1 {service_port}")
                async with connected(ws):
                    await ws.send(f"CONNECT localhost:{echo_port} HTTP/1.1\r\n".encode())
                    await ws.send(b"Host: localhost\r\n\r\ninitial\0data")
                    assert await ws.recv() == b"HTTP/1.1 200 Connection Established\r\n\r\n"
                    assert await ws.recv() == b"initial\0data"
                    await ws.send(b"second")
                    assert await ws.recv() == b"second"

            for request, status in (
                (b"GET / HTTP/1.1\r\n\r\n", b"400"),
                (b"CONNECT host:65536 HTTP/1.1\r\n\r\n", b"400"),
                (b"CONNECT [::1] HTTP/1.1\r\n\r\n", b"400"),
                (b"x" * 16385, b"431"),
            ):
                ws = await open_service("PROXY IPV6 TCP fd00::1 8080")
                await ws.send(request)
                assert (await ws.recv()).startswith(b"HTTP/1.1 " + status)
                await closed(ws)

        # The echo listener is now closed: failure must be reported, not acknowledged.
        ws = await open_service(f"PROXY IPV4 TCP 127.0.0.1 {echo_port}", "PROXY FAILED")
        await closed(ws)
        ws = await open_service("PROXY IPV6 TCP fd00::1 8080")
        await ws.send(f"CONNECT 127.0.0.1:{echo_port} HTTP/1.1\r\n\r\n".encode())
        assert (await ws.recv()).startswith(b"HTTP/1.1 502")
        await closed(ws)

        # Disconnect while operations are outstanding, then check endpoint health.
        for _ in range(10):
            ws = await open_service("PROXY IPV6 TCP fd00::1 53")
            await ws.send(b"localhost ANY")
            await ws.close()
        assert await dns("127.0.0.1", "A") == [ipaddress.ip_address("127.0.0.1")]
    finally:
        process.terminate()
        process.wait(timeout=5)


async def main():
    for enabled in (False, True):
        await asyncio.wait_for(run(sys.argv[1], enabled), timeout=40)
        print(f"PASS: DNS and proxy protocol (proxy enabled={enabled})")


asyncio.run(main())
