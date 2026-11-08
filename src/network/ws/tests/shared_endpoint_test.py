#!/usr/bin/env python3
"""Exercise both WebSocket protocols against a disposable Freeminer server.

Example: python3 shared_endpoint_test.py --url wss://127.0.0.1:45261 --insecure
Use --sctp-url to check a separately configured SCTP listener.
"""
import argparse
import asyncio
import ssl
import struct
from urllib.parse import urlsplit

import websockets


def crc32c(data):
    value = 0xffffffff
    for byte in data:
        value ^= byte
        for _ in range(8):
            value = (value >> 1) ^ (0x82f63b78 if value & 1 else 0)
    return (~value) & 0xffffffff


def packet(port, tag, chunk):
    data = bytearray(struct.pack('!HHII', 45001, port, tag, 0) + chunk)
    data[8:12] = struct.pack('<I', crc32c(data))
    return bytes(data)


async def main(args):
    base = args.url.rstrip('/')
    sctp_url = args.sctp_url or base + '/sctp?test=1'
    port = urlsplit(sctp_url).port
    if not port:
        raise ValueError('the SCTP URL must include its port')
    context = ssl._create_unverified_context() if args.insecure else ssl.create_default_context()

    def connect(url):
        return websockets.connect(url, ssl=context if url.startswith('wss:') else None)

    async with connect(base + '/') as ws:
        await ws.send('NEWADDR')
        reply = await asyncio.wait_for(ws.recv(), 5)
        command, address, passcode, joincode = reply.split()
        assert command == 'ADDR' and address and passcode and joincode
        print('PASS: / NEWADDR')

    async with connect(base + '/ws') as ws:
        await ws.send('BIND ' + passcode + ' UDP 49152')
        assert await asyncio.wait_for(ws.recv(), 5) == 'BIND OK'
        print('PASS: /ws BIND')
        async with connect(sctp_url) as sctp:
            init = struct.pack('!BBHIIHHI', 1, 0, 20, 0x12345678,
                               1048576, 10, 10, 12345)
            await sctp.send(packet(port, 0, init))
            response = await asyncio.wait_for(sctp.recv(), 5)
            assert isinstance(response, bytes) and len(response) >= 32 and response[12] == 2
            server_tag = struct.unpack_from('!I', response, 16)[0]
            chunk_length = struct.unpack_from('!H', response, 14)[0]
            offset, cookie = 32, None
            while offset < 12 + chunk_length:
                kind, length = struct.unpack_from('!HH', response, offset)
                assert length >= 4
                if kind == 7:
                    cookie = response[offset + 4:offset + length]
                    break
                offset += (length + 3) & ~3
            assert cookie, 'INIT_ACK contained no state cookie'
            chunk = struct.pack('!BBH', 10, 0, 4 + len(cookie)) + cookie
            chunk += bytes((-len(chunk)) % 4)
            await sctp.send(packet(port, server_tag, chunk))
            response = await asyncio.wait_for(sctp.recv(), 5)
            assert isinstance(response, bytes) and response[12] == 11
            print('PASS: SCTP INIT and COOKIE handshake')

            name = b'SharedSctpTest'
            payload = struct.pack('!HBHHHH', 2, 29, 0, 37, 53, len(name)) + name
            chunk = struct.pack('!BBHIHHI', 0, 3, 16 + len(payload), 12345, 2, 0, 0) + payload
            chunk += bytes((-len(chunk)) % 4)
            await sctp.send(packet(port, server_tag, chunk))
            got_hello = False
            for _ in range(10):
                response = await asyncio.wait_for(sctp.recv(), 5)
                offset = 12
                while offset + 4 <= len(response):
                    kind, flags, length = struct.unpack_from('!BBH', response, offset)
                    assert length >= 4
                    if kind == 0 and length >= 18:
                        got_hello |= struct.unpack_from('!H', response, offset + 16)[0] == 2
                    offset += (length + 3) & ~3
                if got_hello:
                    break
            assert got_hello, 'server did not return TOCLIENT_HELLO'
            print('PASS: SCTP game INIT and HELLO')

            await sctp.send('unexpected text')
            try:
                await asyncio.wait_for(sctp.recv(), 5)
                raise AssertionError('text SCTP frame was accepted')
            except websockets.ConnectionClosed as exc:
                assert exc.code == 1003
            print('PASS: SCTP rejects text frames')
        pong = await ws.ping()
        await asyncio.wait_for(pong, 5)
        print('PASS: datagram session survives SCTP close')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--url', required=True, help='ordinary WebSocket server URL')
    parser.add_argument('--sctp-url', help='separate SCTP WebSocket URL')
    parser.add_argument('--insecure', action='store_true', help='accept a test TLS certificate')
    args = parser.parse_args()
    asyncio.run(main(args))
    if not args.sctp_url:
        print('Checking the legacy SCTP root path')
        args.sctp_url = args.url.rstrip('/') + '/'
        asyncio.run(main(args))
