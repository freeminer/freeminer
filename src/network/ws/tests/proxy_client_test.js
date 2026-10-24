// Exercise the actual browser shim against the C++ protocol test endpoint.
// node proxy_client_test.js /path/to/proxy_server /path/to/proxy.js
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const {spawn} = require('node:child_process');
const {once} = require('node:events');
const {createInterface} = require('node:readline');

async function main()
{
    const server = spawn(process.argv[2], ['disabled'], {stdio: ['ignore', 'pipe', 'inherit']});
    const timer = setTimeout(() => {
        console.error('Client protocol test timed out');
        server.kill();
        process.exit(1);
    }, 15000);
    const links = [];
    try {
        const lines = createInterface({input: server.stdout});
        const [port] = await once(lines, 'line');
        const c = {WebSocket, console, Uint8Array, ArrayBuffer, DataView};
        c.self = c;
        vm.createContext(c);
        vm.runInContext(fs.readFileSync(process.argv[3], 'utf8'), c);
        c.setProxy(`ws://127.0.0.1:${port}`);
        const link = new c.ProxyLink(12345, true);
        links.push(link);
        let opened;
        const ready = new Promise((resolve, reject) => {
            opened = resolve;
            link.onerror = reject;
        });
        link.onopen = opened;
        const packet = new Uint8Array([0x4f, 0x45, 0x74, 3, 0, 255]);
        const echoed = new Promise(resolve => {
            link.onmessage = (data, ip, srcPort) => resolve([Array.from(new Uint8Array(data)), ip, srcPort]);
        });
        // Queue immediately, before NEWADDR or BIND has completed.
        link.sendto(packet, '192.0.2.10', Number(port));
        packet.fill(0);
        await ready;
        assert.ok(c.assignment.address.startsWith('fd00:'));
        assert.equal(c.assignment.passcode.length, 32);
        assert.deepEqual(await echoed, [[0x4f, 0x45, 0x74, 3, 0, 255], '192.0.2.10', Number(port)]);

        const second = new c.ProxyLink(12346, true);
        links.push(second);
        await new Promise(resolve => { second.onopen = resolve; });
        const forwarded = new Promise(resolve => {
            second.onmessage = (data, ip, srcPort) => resolve([Array.from(new Uint8Array(data)), ip, srcPort]);
        });
        link.sendto(new Uint8Array([9, 8]), c.assignment.address, 12346);
        assert.deepEqual(await forwarded, [[9, 8], c.assignment.address, 12345]);

        // A stale passcode is retried by the real shim with a fresh NEWADDR.
        c.assignment = {address: 'fd00::dead', passcode: 'expired', joincode: 'expired'};
        const rebound = new c.ProxyLink(12347, true);
        links.push(rebound);
        await new Promise(resolve => { rebound.onopen = resolve; });
        assert.equal(rebound.rebound, true);
        assert.notEqual(c.assignment.passcode, 'expired');
        console.log('PASS: actual JavaScript NEWADDR/BIND, queued game packets, virtual UDP and stale-assignment retry');
    } finally {
        for (const link of links) link.close();
        clearTimeout(timer);
        server.kill();
    }
}
main().catch(err => { console.error(err); process.exitCode = 1; });
