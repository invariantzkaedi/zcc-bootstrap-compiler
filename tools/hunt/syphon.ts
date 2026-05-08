import { WebSocket } from 'ws';
import * as net from 'net';
import { ethers } from 'ethers';

const IPC_PATH = '/tmp/warden_syphon.sock';
const WS_RPC = process.env.WS_RPC || 'ws://127.0.0.1:8546';

// Reconnectable IPC client
let client: net.Socket;

function connectIPC() {
    client = net.createConnection(IPC_PATH);
    client.on('connect', () => {
        console.log('\x1b[38;5;51m[SYPHON] Connected to Apex Orchestrator over IPC\x1b[0m');
    });
    client.on('error', (err) => {
        // Suppress errors if orchestrator isn't up yet
    });
    client.on('close', () => {
        setTimeout(connectIPC, 1000);
    });
}

connectIPC();

async function startSyphon() {
    console.log('\x1b[38;5;17m[MEMPOOL]\x1b[38;5;51m Synchronizing with Execution Layer...\x1b[0m');
    
    const provider = new ethers.WebSocketProvider(WS_RPC);
    
    let tps = 0;
    setInterval(() => {
        console.log(`\x1b[38;5;17m[TELEMETRY]\x1b[38;5;51m Mempool TPS: \x1b[38;5;199m${tps}\x1b[0m`);
        tps = 0;
    }, 1000);

    provider.on('pending', async (txHash: string) => {
        tps++;
        try {
            const tx = await provider.getTransaction(txHash);
            if (!tx || !tx.to) return; // Ignore contract creations
            
            const code = await provider.getCode(tx.to);
            if (code === '0x') return; // Ignore EOAs
            
            // Filter out known safe/minimal proxies by size
            if (code.length < 50) return; 
            
            const payload = JSON.stringify({
                hash: tx.hash,
                to: tx.to,
                bytecode: code,
                timestamp: Date.now()
            });
            
            if (client && !client.destroyed) {
                client.write(payload + '\n');
            }
        } catch (e) {
            // Ignore fetch errors due to rapid state changes
        }
    });
}

startSyphon().catch(console.error);
