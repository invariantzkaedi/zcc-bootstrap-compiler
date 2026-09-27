const fs = require('fs');

async function run() {
    const file = process.argv[2] || '/tmp/probe.wasm';
    const bytes = fs.readFileSync(file);
    const wasi = {
        proc_exit: (code) => { process.exit(code); },
        fd_write: (fd, iovs, iovs_len, nwritten) => { return 0; }
    };
    const mod = await WebAssembly.instantiate(bytes, { wasi_snapshot_preview1: wasi, env: wasi });
    const instance = mod.instance;
    console.log('[WASM RUNNER] Module loaded successfully.');
    if (instance.exports.add) {
        console.log('[WASM RUNNER] add(40, 2) =', instance.exports.add(40, 2));
    }
    if (instance.exports.main) {
        console.log('[WASM RUNNER] main() =', instance.exports.main());
    }
}

run().catch(err => {
    console.error('[WASM ERROR]', err);
    process.exit(1);
});
