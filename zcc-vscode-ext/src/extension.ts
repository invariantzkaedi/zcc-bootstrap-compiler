/**
 * ZCC Rust Frontend — VSCode Extension
 *
 * Transport hierarchy (fastest to slowest):
 *   1. Native Messaging → SHM ring (future: when nm host is installed)
 *   2. WebSocket → Rust frontend → SHM ring  ← primary path today
 *   3. Error / graceful degradation
 *
 * The Rust frontend (main.rs) receives JSON over WS, parses it into a
 * WireInitNodeBuilder tree, serializes into the SHM ring, and ZCC's
 * zcc_rust_shm_entry() picks it up at memory speed via zcc_build_from_wire().
 */

import * as vscode from 'vscode';
import WebSocket from 'ws';

// ── State ──────────────────────────────────────────────────────────────────

let socket: WebSocket | null = null;
let statusBar: vscode.StatusBarItem;
let reconnectTimer: NodeJS.Timeout | null = null;
let isConnecting = false;

// ── Activation ─────────────────────────────────────────────────────────────

export function activate(context: vscode.ExtensionContext): void {
    // Status bar button — always visible when a C/C++ file is open
    statusBar = vscode.window.createStatusBarItem(vscode.StatusBarAlignment.Right, 100);
    statusBar.command = 'zcc.connectFrontend';
    setStatus('disconnected');
    statusBar.show();
    context.subscriptions.push(statusBar);

    // ── Commands ────────────────────────────────────────────────────────────

    context.subscriptions.push(
        vscode.commands.registerCommand('zcc.connectFrontend', () => connect(context)),
        vscode.commands.registerCommand('zcc.disconnectFrontend', () => disconnect()),
        vscode.commands.registerCommand('zcc.injectTesseract', () => injectCurrentFile()),
    );

    // ── Auto-inject on save (opt-in) ────────────────────────────────────────

    context.subscriptions.push(
        vscode.workspace.onDidSaveTextDocument((doc) => {
            const cfg = vscode.workspace.getConfiguration('zcc');
            if (!cfg.get<boolean>('autoInjectOnSave')) return;
            if (doc.languageId !== 'c' && doc.languageId !== 'cpp') return;
            injectCurrentFile();
        })
    );

    // Auto-connect on startup if a C file is already open
    const hasCFile = vscode.workspace.textDocuments.some(
        (d) => d.languageId === 'c' || d.languageId === 'cpp'
    );
    if (hasCFile) {
        connect(context);
    }
}

export function deactivate(): void {
    disconnect();
    statusBar?.dispose();
}

// ── WebSocket Connection ────────────────────────────────────────────────────

function connect(_context: vscode.ExtensionContext): void {
    if (isConnecting || (socket && socket.readyState === WebSocket.OPEN)) return;

    const cfg = vscode.workspace.getConfiguration('zcc');
    const url = cfg.get<string>('frontendUrl') ?? 'ws://127.0.0.1:9001';
    const secret = cfg.get<string>('secret') ?? 'zcc-flow-state-2026';

    isConnecting = true;
    setStatus('connecting');

    socket = new WebSocket(url, { headers: { 'X-Zcc-Secret': secret } });

    socket.on('open', () => {
        isConnecting = false;
        setStatus('connected');
        vscode.window.setStatusBarMessage('$(check) ZCC frontend connected', 3000);
        clearReconnectTimer();
    });

    socket.on('message', (raw) => {
        try {
            const msg = JSON.parse(raw.toString()) as ZccMessage;
            handleServerMessage(msg);
        } catch (_) { /* ignore malformed */ }
    });

    socket.on('error', (err) => {
        isConnecting = false;
        setStatus('error');
        console.error('[ZCC] WS error:', err.message);
    });

    socket.on('close', (code, reason) => {
        isConnecting = false;
        setStatus('disconnected');
        console.log(`[ZCC] WS closed ${code}: ${reason}`);
        scheduleReconnect();
    });
}

function disconnect(): void {
    clearReconnectTimer();
    if (socket) {
        socket.removeAllListeners();
        socket.close(1000, 'user disconnect');
        socket = null;
    }
    setStatus('disconnected');
}

function scheduleReconnect(): void {
    clearReconnectTimer();
    reconnectTimer = setTimeout(() => {
        if (!socket || socket.readyState !== WebSocket.OPEN) {
            connect({} as vscode.ExtensionContext);
        }
    }, 5000);
}

function clearReconnectTimer(): void {
    if (reconnectTimer) {
        clearTimeout(reconnectTimer);
        reconnectTimer = null;
    }
}

// ── Injection ───────────────────────────────────────────────────────────────

async function injectCurrentFile(): Promise<void> {
    const editor = vscode.window.activeTextEditor;
    if (!editor) {
        vscode.window.showWarningMessage('ZCC: No active editor');
        return;
    }
    if (editor.document.languageId !== 'c' && editor.document.languageId !== 'cpp') {
        vscode.window.showWarningMessage('ZCC: Active file is not C/C++');
        return;
    }

    if (!socket || socket.readyState !== WebSocket.OPEN) {
        vscode.window.showErrorMessage('ZCC: Not connected — run "ZCC: Connect Rust Frontend" first');
        return;
    }

    // Parse the current selection or whole document into a wire tree.
    const text = editor.selection.isEmpty
        ? editor.document.getText()
        : editor.document.getText(editor.selection);

    const payload = buildPayload(text, editor.document.fileName);

    try {
        socket.send(JSON.stringify(payload));
        vscode.window.setStatusBarMessage('$(rocket) ZCC: tree injected via SHM', 4000);
    } catch (err) {
        vscode.window.showErrorMessage(`ZCC inject failed: ${(err as Error).message}`);
    }
}

// ── Wire Payload Builder ────────────────────────────────────────────────────
// Builds a JSON message the Rust frontend understands. The Rust side parses
// this into a WireInitNodeBuilder tree and writes it to the SHM ring.

interface WireNode {
    kind: 'list' | 'value' | 'designated_field' | 'designated_index';
    children?: WireNode[];
    vkind?: 'i64' | 'f64' | 'str' | 'ident';
    value?: number | string;
    field?: string;
    index?: number;
}

interface ZccInjectPayload {
    type: 'inject';
    file: string;
    tree: WireNode;
}

type ZccMessage =
    | { type: 'ack'; slots_used: number; drop_count: number }
    | { type: 'error'; message: string }
    | { type: 'status'; connected: boolean };

function buildPayload(source: string, filename: string): ZccInjectPayload {
    // Heuristic: detect top-level initializer expressions in the source.
    // If the source contains a braced initializer, send it as a list.
    // Otherwise, treat the whole selection as a single identifier value.
    //
    // In production this would use a proper C expression parser.
    // For the MVP, we detect `= { ... }` patterns and build a list node.

    const initMatch = source.match(/=\s*\{([\s\S]*)\}/);
    if (initMatch) {
        const inner = initMatch[1].trim();
        const elements = splitTopLevel(inner);
        return {
            type: 'inject',
            file: filename,
            tree: {
                kind: 'list',
                children: elements.map(parseElement),
            },
        };
    }

    // Fallback: send as opaque identifier
    return {
        type: 'inject',
        file: filename,
        tree: {
            kind: 'value',
            vkind: 'ident',
            value: source.trim().split(/\s+/)[0] ?? 'unknown',
        },
    };
}

/** Split a comma-separated initializer list respecting nested braces. */
function splitTopLevel(src: string): string[] {
    const parts: string[] = [];
    let depth = 0;
    let start = 0;
    for (let i = 0; i < src.length; i++) {
        if (src[i] === '{') depth++;
        else if (src[i] === '}') depth--;
        else if (src[i] === ',' && depth === 0) {
            parts.push(src.slice(start, i).trim());
            start = i + 1;
        }
    }
    const tail = src.slice(start).trim();
    if (tail) parts.push(tail);
    return parts.filter(Boolean);
}

function parseElement(elem: string): WireNode {
    elem = elem.trim();

    // Designated field: .name = ...
    const fieldMatch = elem.match(/^\s*\.\s*(\w+)\s*=\s*([\s\S]+)$/);
    if (fieldMatch) {
        return {
            kind: 'designated_field',
            field: fieldMatch[1],
            children: [parseElement(fieldMatch[2])],
        };
    }

    // Designated index: [N] = ...
    const idxMatch = elem.match(/^\s*\[\s*(\d+)\s*\]\s*=\s*([\s\S]+)$/);
    if (idxMatch) {
        return {
            kind: 'designated_index',
            index: parseInt(idxMatch[1], 10),
            children: [parseElement(idxMatch[2])],
        };
    }

    // Nested list
    if (elem.startsWith('{') && elem.endsWith('}')) {
        const inner = elem.slice(1, -1).trim();
        return {
            kind: 'list',
            children: splitTopLevel(inner).map(parseElement),
        };
    }

    // Numeric literal
    const num = Number(elem);
    if (!isNaN(num) && elem !== '') {
        return {
            kind: 'value',
            vkind: elem.includes('.') ? 'f64' : 'i64',
            value: num,
        };
    }

    // String literal
    if (elem.startsWith('"') && elem.endsWith('"')) {
        return { kind: 'value', vkind: 'str', value: elem.slice(1, -1) };
    }

    // Identifier / expression
    return { kind: 'value', vkind: 'ident', value: elem };
}

// ── Server Message Handler ──────────────────────────────────────────────────

function handleServerMessage(msg: ZccMessage): void {
    switch (msg.type) {
        case 'ack':
            if (msg.drop_count > 0) {
                vscode.window.setStatusBarMessage(
                    `$(warning) ZCC SHM: ${msg.drop_count} dropped slots`,
                    3000
                );
            } else {
                vscode.window.setStatusBarMessage(
                    `$(check) ZCC SHM: slot ${msg.slots_used} consumed`,
                    2000
                );
            }
            break;
        case 'error':
            vscode.window.showErrorMessage(`ZCC backend error: ${msg.message}`);
            break;
        case 'status':
            setStatus(msg.connected ? 'connected' : 'disconnected');
            break;
    }
}

// ── Status Bar Helpers ──────────────────────────────────────────────────────

type ConnectionState = 'connected' | 'connecting' | 'disconnected' | 'error';

function setStatus(state: ConnectionState): void {
    switch (state) {
        case 'connected':
            statusBar.text = '$(radio-tower) ZCC';
            statusBar.tooltip = 'ZCC Rust Frontend connected — click to disconnect';
            statusBar.command = 'zcc.disconnectFrontend';
            statusBar.backgroundColor = undefined;
            break;
        case 'connecting':
            statusBar.text = '$(sync~spin) ZCC';
            statusBar.tooltip = 'Connecting to ZCC Rust Frontend…';
            statusBar.command = undefined;
            statusBar.backgroundColor = undefined;
            break;
        case 'disconnected':
            statusBar.text = '$(plug) ZCC';
            statusBar.tooltip = 'ZCC Rust Frontend disconnected — click to connect';
            statusBar.command = 'zcc.connectFrontend';
            statusBar.backgroundColor = new vscode.ThemeColor('statusBarItem.warningBackground');
            break;
        case 'error':
            statusBar.text = '$(error) ZCC';
            statusBar.tooltip = 'ZCC connection error — will retry in 5s';
            statusBar.command = 'zcc.connectFrontend';
            statusBar.backgroundColor = new vscode.ThemeColor('statusBarItem.errorBackground');
            break;
    }
}
