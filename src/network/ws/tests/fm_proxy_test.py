#!/usr/bin/env python3
"""Protocol integration tests. Pass the compiled fm_proxy_server executable."""
import asyncio
from contextlib import asynccontextmanager
import ipaddress
import subprocess
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

        # A resolved game address on this listener must select the game route,
        # including the public address behind NAT and IPv4-mapped IPv6.
        for family, host in (("IPV4", "127.0.0.1"), ("IPV6", "::ffff:127.0.0.1"),
                             ("IPV4", "192.0.2.10"), ("IPV4", "10.0.0.1")):
            ws = await open_service(f"PROXY {family} TCP {host} {port}", "GAME OK")
            async with connected(ws):
                packet = bytes.fromhex("4f45740300000001")
                await ws.send(packet)
                assert await ws.recv() == packet

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
