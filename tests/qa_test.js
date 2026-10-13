const assert = require('node:assert/strict');
const net = require('node:net');
const { once } = require('node:events');
const { spawn } = require('node:child_process');

async function freePort() {
  const listener = net.createServer();
  await new Promise((resolve, reject) => {
    listener.once('error', reject);
    listener.listen(0, '127.0.0.1', resolve);
  });
  const { port } = listener.address();
  await new Promise(resolve => listener.close(resolve));
  return port;
}

async function waitForServer(server, base) {
  let output = '';
  server.stdout.on('data', chunk => { output += chunk; });
  server.stderr.on('data', chunk => { output += chunk; });
  const deadline = Date.now() + 5000;
  while (Date.now() < deadline) {
    if (server.exitCode !== null || server.signalCode !== null) {
      throw new Error(`QA server exited early (${server.exitCode || server.signalCode}): ${output}`);
    }
    try {
      const response = await fetch(base);
      if (response.ok) return;
    } catch (_) {
      // The listener may need another event-loop turn before accepting requests.
    }
    await new Promise(resolve => setTimeout(resolve, 50));
  }
  throw new Error(`QA server did not start: ${output}`);
}

async function runLanTest(base) {
  const child = spawn(process.execPath, ['tests/lan_test.js', base], { stdio: 'inherit' });
  const [code, signal] = await once(child, 'exit');
  assert.equal(signal, null, `LAN test stopped by ${signal}`);
  assert.equal(code, 0, 'LAN integration test failed');
}

(async () => {
  const port = await freePort();
  const server = spawn('./build/mike-root_server', [String(port), 'web'], {
    stdio: ['ignore', 'pipe', 'pipe']
  });
  const base = `http://127.0.0.1:${port}`;
  try {
    await waitForServer(server, base);
    await runLanTest(base);
    console.log('Full QA suite passed.');
  } finally {
    if (server.exitCode === null && server.signalCode === null) {
      server.kill('SIGTERM');
      await Promise.race([once(server, 'exit'), new Promise(resolve => setTimeout(resolve, 2000))]);
    }
  }
})().catch(error => {
  console.error(error);
  process.exitCode = 1;
});
