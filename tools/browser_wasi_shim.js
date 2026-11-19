#!/usr/bin/env node
/**
 * Zero-Dependency Pure JavaScript WASI Preview1 In-Memory VFS Runtime
 * ==================================================================
 * Implements WASI `wasi_snapshot_preview1` (all filesystem, CLI args, env,
 * clock, stdio, and exit imports) over an in-memory Map VFS for both Browser
 * WebAssembly environments and standalone Node verification without `node:wasi`.
 */

"use strict";

const EBADF = 8;
const EINVAL = 28;
const EEXIST = 20;
const ENOENT = 44;
const ESPIPE = 70;

const O_CREAT = 1;
const O_DIRECTORY = 2;
const O_EXCL = 4;
const O_TRUNC = 8;
const FDFLAG_APPEND = 1;

const FILETYPE_CHARACTER_DEVICE = 2;
const FILETYPE_DIRECTORY = 3;
const FILETYPE_REGULAR_FILE = 4;

const utf8Enc = new TextEncoder();
const utf8Dec = new TextDecoder("utf-8", { fatal: false });

class WasiProcExit extends Error {
  constructor(code) {
    super(`WASI proc_exit(${code | 0})`);
    this.code = code | 0;
  }
}

function normalizeVfsPath(p) {
  let s = String(p || "").replace(/\\/g, "/");
  const parts = [];
  for (const seg of s.split("/")) {
    if (!seg || seg === ".") continue;
    if (seg === "..") {
      if (parts.length > 0) parts.pop();
    } else {
      parts.push(seg);
    }
  }
  return parts.join("/");
}

class BrowserWasiShim {
  constructor(options = {}) {
    this.args = Array.isArray(options.args) ? options.args.map(String) : ["program.wasm"];
    this.envObj = options.env && typeof options.env === "object" ? options.env : {};
    this.preopenDir = options.preopenDir !== undefined ? String(options.preopenDir) : ".";
    this.memory = null;
    this.exitCode = 0;

    this.vfs = new Map();
    if (options.files && typeof options.files === "object") {
      for (const [k, v] of Object.entries(options.files)) {
        const key = normalizeVfsPath(k);
        const bytes = typeof v === "string" ? utf8Enc.encode(v) : new Uint8Array(v);
        this.vfs.set(key, bytes);
      }
    }

    const stdinBytes = typeof options.stdin === "string"
      ? utf8Enc.encode(options.stdin)
      : (options.stdin ? new Uint8Array(options.stdin) : new Uint8Array(0));

    this.stdoutChunks = [];
    this.stderrChunks = [];
    this.nextFd = 4;
    this.fds = new Map();
    this.fds.set(0, { kind: "stdin", data: stdinBytes, pos: 0 });
    this.fds.set(1, { kind: "stdout" });
    this.fds.set(2, { kind: "stderr" });
    this.fds.set(3, { kind: "dir", path: this.preopenDir });
  }

  setMemory(memory) {
    this.memory = memory;
  }

  _view() {
    if (!this.memory) throw new Error("BrowserWasiShim memory not bound");
    return new DataView(this.memory.buffer);
  }

  _u8() {
    if (!this.memory) throw new Error("BrowserWasiShim memory not bound");
    return new Uint8Array(this.memory.buffer);
  }

  _envStrings() {
    return Object.entries(this.envObj).map(([k, v]) => `${k}=${v}`);
  }

  getStdout() {
    return this._concatText(this.stdoutChunks);
  }

  getStderr() {
    return this._concatText(this.stderrChunks);
  }

  _concatText(chunks) {
    let total = 0;
    for (const c of chunks) total += c.length;
    const out = new Uint8Array(total);
    let off = 0;
    for (const c of chunks) {
      out.set(c, off);
      off += c.length;
    }
    return utf8Dec.decode(out);
  }

  getFile(path) {
    return this.vfs.get(normalizeVfsPath(path)) || null;
  }

  getFileText(path) {
    const b = this.getFile(path);
    return b ? utf8Dec.decode(b) : null;
  }

  setFile(path, content) {
    const bytes = typeof content === "string" ? utf8Enc.encode(content) : new Uint8Array(content);
    this.vfs.set(normalizeVfsPath(path), bytes);
  }

  _syncFdToVfs(entry) {
    if (entry && entry.kind === "file" && !entry.sharedReadOnly) {
      this.vfs.set(entry.path, entry.data.slice(0, entry.len));
    }
  }

  getImports() {
    const self = this;
    return {
      wasi_snapshot_preview1: {
        args_sizes_get(argcPtr, argvBufSizePtr) {
          const dv = self._view();
          let bufBytes = 0;
          for (const a of self.args) bufBytes += utf8Enc.encode(a).length + 1;
          dv.setUint32(argcPtr >>> 0, self.args.length >>> 0, true);
          dv.setUint32(argvBufSizePtr >>> 0, bufBytes >>> 0, true);
          return 0;
        },

        args_get(argvPtr, argvBufPtr) {
          const dv = self._view();
          const u8 = self._u8();
          let pPtr = argvPtr >>> 0;
          let bPtr = argvBufPtr >>> 0;
          for (const a of self.args) {
            const bytes = utf8Enc.encode(a);
            dv.setUint32(pPtr, bPtr, true);
            u8.set(bytes, bPtr);
            u8[bPtr + bytes.length] = 0;
            pPtr += 4;
            bPtr += bytes.length + 1;
          }
          return 0;
        },

        environ_sizes_get(envcPtr, envBufSizePtr) {
          const dv = self._view();
          const envs = self._envStrings();
          let bufBytes = 0;
          for (const e of envs) bufBytes += utf8Enc.encode(e).length + 1;
          dv.setUint32(envcPtr >>> 0, envs.length >>> 0, true);
          dv.setUint32(envBufSizePtr >>> 0, bufBytes >>> 0, true);
          return 0;
        },

        environ_get(envPtr, envBufPtr) {
          const dv = self._view();
          const u8 = self._u8();
          const envs = self._envStrings();
          let pPtr = envPtr >>> 0;
          let bPtr = envBufPtr >>> 0;
          for (const e of envs) {
            const bytes = utf8Enc.encode(e);
            dv.setUint32(pPtr, bPtr, true);
            u8.set(bytes, bPtr);
            u8[bPtr + bytes.length] = 0;
            pPtr += 4;
            bPtr += bytes.length + 1;
          }
          return 0;
        },

        clock_time_get(_id, _precision, outTimePtr) {
          const dv = self._view();
          const ms = Date.now();
          const ns = BigInt(ms) * 1000000n;
          dv.setBigUint64(outTimePtr >>> 0, ns, true);
          return 0;
        },

        fd_close(fd) {
          const entry = self.fds.get(fd | 0);
          if (!entry) return EBADF;
          self._syncFdToVfs(entry);
          if ((fd | 0) > 3) self.fds.delete(fd | 0);
          return 0;
        },

        fd_fdstat_get(fd, statPtr) {
          const entry = self.fds.get(fd | 0);
          if (!entry) return EBADF;
          const dv = self._view();
          const u8 = self._u8();
          const p = statPtr >>> 0;
          u8.fill(0, p, p + 24);
          let ftype = FILETYPE_REGULAR_FILE;
          if (entry.kind === "dir") ftype = FILETYPE_DIRECTORY;
          else if (entry.kind === "stdin" || entry.kind === "stdout" || entry.kind === "stderr") {
            ftype = FILETYPE_CHARACTER_DEVICE;
          }
          dv.setUint8(p, ftype);
          dv.setBigUint64(p + 8, 0xFFFFFFFFn, true);
          dv.setBigUint64(p + 16, 0xFFFFFFFFn, true);
          return 0;
        },

        fd_prestat_get(fd, prestatPtr) {
          const entry = self.fds.get(fd | 0);
          if (!entry || entry.kind !== "dir") return EBADF;
          const dv = self._view();
          const nameLen = utf8Enc.encode(entry.path).length;
          dv.setUint32(prestatPtr >>> 0, 0, true); /* __WASI_PREOPENTYPE_DIR */
          dv.setUint32((prestatPtr >>> 0) + 4, nameLen >>> 0, true);
          return 0;
        },

        fd_prestat_dir_name(fd, pathPtr, pathLen) {
          const entry = self.fds.get(fd | 0);
          if (!entry || entry.kind !== "dir") return EBADF;
          const u8 = self._u8();
          const bytes = utf8Enc.encode(entry.path);
          const n = Math.min(bytes.length, pathLen >>> 0);
          u8.set(bytes.subarray(0, n), pathPtr >>> 0);
          return 0;
        },

        fd_read(fd, iovsPtr, iovsLen, nreadPtr) {
          const entry = self.fds.get(fd | 0);
          if (!entry) return EBADF;
          if (entry.kind !== "stdin" && entry.kind !== "file") return EBADF;
          const dv = self._view();
          const u8 = self._u8();
          const srcLen = entry.kind === "file" ? entry.len : entry.data.length;
          let total = 0;
          for (let i = 0; i < (iovsLen >>> 0); i++) {
            const base = dv.getUint32((iovsPtr >>> 0) + i * 8, true);
            const len = dv.getUint32((iovsPtr >>> 0) + i * 8 + 4, true);
            const avail = Math.max(0, srcLen - entry.pos);
            const take = Math.min(len, avail);
            if (take > 0) {
              u8.set(entry.data.subarray(entry.pos, entry.pos + take), base);
              entry.pos += take;
              total += take;
            }
            if (take < len) break;
          }
          dv.setUint32(nreadPtr >>> 0, total >>> 0, true);
          return 0;
        },

        fd_seek(fd, offsetI64, whence, newOffsetPtr) {
          const entry = self.fds.get(fd | 0);
          if (!entry || entry.kind !== "file") return entry ? ESPIPE : EBADF;
          const off = Number(offsetI64);
          let nextPos = entry.pos;
          if (whence === 0) nextPos = off;
          else if (whence === 1) nextPos = entry.pos + off;
          else if (whence === 2) nextPos = entry.len + off;
          else return EINVAL;
          if (nextPos < 0) return EINVAL;
          entry.pos = nextPos;
          self._view().setBigUint64(newOffsetPtr >>> 0, BigInt(nextPos), true);
          return 0;
        },

        fd_write(fd, iovsPtr, iovsLen, nwrittenPtr) {
          const entry = self.fds.get(fd | 0);
          if (!entry) return EBADF;
          const dv = self._view();
          const u8 = self._u8();
          let total = 0;
          for (let i = 0; i < (iovsLen >>> 0); i++) {
            const base = dv.getUint32((iovsPtr >>> 0) + i * 8, true);
            const len = dv.getUint32((iovsPtr >>> 0) + i * 8 + 4, true);
            if (len === 0) continue;
            const slice = u8.slice(base, base + len);
            if (entry.kind === "stdout") {
              self.stdoutChunks.push(slice);
            } else if (entry.kind === "stderr") {
              self.stderrChunks.push(slice);
            } else if (entry.kind === "file") {
              if (entry.sharedReadOnly) {
                const copy = new Uint8Array(Math.max(256, entry.len));
                copy.set(entry.data.subarray(0, entry.len), 0);
                entry.data = copy;
                entry.sharedReadOnly = false;
              }
              if (entry.append) entry.pos = entry.len;
              const endPos = entry.pos + slice.length;
              if (endPos > entry.data.length) {
                let newCap = Math.max(256, entry.data.length * 2);
                while (newCap < endPos) newCap *= 2;
                const grown = new Uint8Array(newCap);
                grown.set(entry.data.subarray(0, entry.len), 0);
                entry.data = grown;
              }
              entry.data.set(slice, entry.pos);
              entry.pos = endPos;
              if (endPos > entry.len) entry.len = endPos;
            } else {
              return EBADF;
            }
            total += len;
          }
          if (entry.kind === "file") self._syncFdToVfs(entry);
          dv.setUint32(nwrittenPtr >>> 0, total >>> 0, true);
          return 0;
        },

        path_open(dirfd, _dirflags, pathPtr, pathLen, oflags, _rBase, _rInh, fdflags, outFdPtr) {
          const dir = self.fds.get(dirfd | 0);
          if (!dir || dir.kind !== "dir") return EBADF;
          const u8 = self._u8();
          const rawPath = utf8Dec.decode(u8.subarray(pathPtr >>> 0, (pathPtr >>> 0) + (pathLen >>> 0)));
          const key = normalizeVfsPath(rawPath);
          const exists = self.vfs.has(key);

          if ((oflags & O_DIRECTORY) !== 0) {
            return EINVAL;
          }
          if (!exists && (oflags & O_CREAT) === 0) {
            return ENOENT;
          }
          if (exists && (oflags & O_CREAT) !== 0 && (oflags & O_EXCL) !== 0) {
            return EEXIST;
          }

          let initBytes;
          let initLen;
          let sharedReadOnly = false;
          if (!exists || (oflags & O_TRUNC) !== 0) {
            initBytes = new Uint8Array(256);
            initLen = 0;
            self.vfs.set(key, new Uint8Array(0));
          } else {
            initBytes = self.vfs.get(key);
            initLen = initBytes.length;
            sharedReadOnly = true;
          }
          const fd = self.nextFd++;
          const isAppend = (fdflags & FDFLAG_APPEND) !== 0;
          self.fds.set(fd, {
            kind: "file",
            path: key,
            data: initBytes,
            len: initLen,
            pos: isAppend ? initLen : 0,
            append: isAppend,
            sharedReadOnly,
          });
          self._view().setInt32(outFdPtr >>> 0, fd, true);
          return 0;
        },

        path_unlink_file(dirfd, pathPtr, pathLen) {
          const dir = self.fds.get(dirfd | 0);
          if (!dir || dir.kind !== "dir") return EBADF;
          const u8 = self._u8();
          const rawPath = utf8Dec.decode(u8.subarray(pathPtr >>> 0, (pathPtr >>> 0) + (pathLen >>> 0)));
          const key = normalizeVfsPath(rawPath);
          if (!self.vfs.has(key)) return ENOENT;
          self.vfs.delete(key);
          return 0;
        },

        proc_exit(code) {
          self.exitCode = code | 0;
          throw new WasiProcExit(self.exitCode);
        },
      },
    };
  }

  async instantiate(wasmBytes) {
    if (!WebAssembly.validate(wasmBytes)) {
      throw new Error("WebAssembly.validate() failed");
    }
    const { instance } = await WebAssembly.instantiate(wasmBytes, this.getImports());
    if (instance.exports.memory) {
      this.setMemory(instance.exports.memory);
    }
    return { instance, shim: this };
  }

  async run(wasmBytes) {
    const { instance } = await this.instantiate(wasmBytes);
    let exitCode = 0;
    try {
      if (instance.exports._start) {
        instance.exports._start();
      } else if (instance.exports.main) {
        exitCode = instance.exports.main() | 0;
      }
    } catch (err) {
      if (err instanceof WasiProcExit) {
        exitCode = err.code | 0;
      } else {
        throw err;
      }
    }
    this.exitCode = exitCode;
    return {
      exitCode,
      stdout: this.getStdout(),
      stderr: this.getStderr(),
      vfs: this.vfs,
    };
  }
}

if (typeof module !== "undefined" && module.exports) {
  module.exports = { BrowserWasiShim, WasiProcExit, normalizeVfsPath };
}
if (typeof globalThis !== "undefined") {
  globalThis.BrowserWasiShim = BrowserWasiShim;
}

if (typeof require !== "undefined" && typeof module !== "undefined" && require.main === module) {
  const fs = require("fs");
  const path = require("path");
  (async () => {
    const argv = process.argv.slice(2);
    let wasmPath = null;
    let vfsRoot = null;
    let stdinData = "";
    const wasmArgs = [];
    for (let i = 0; i < argv.length; i++) {
      const a = argv[i];
      if (a === "--wasm" && i + 1 < argv.length) wasmPath = argv[++i];
      else if (a === "--vfs-dir" && i + 1 < argv.length) vfsRoot = argv[++i];
      else if (a === "--stdin" && i + 1 < argv.length) stdinData = argv[++i];
      else if (a === "--") {
        wasmArgs.push(...argv.slice(i + 1));
        break;
      }
    }
    if (!wasmPath) {
      console.error("Usage: browser_wasi_shim.js --wasm <mod.wasm> [--vfs-dir <dir>] [-- <args...>]");
      process.exit(2);
    }
    const files = {};
    if (vfsRoot && fs.existsSync(vfsRoot)) {
      const walk = (dir, rel) => {
        for (const ent of fs.readdirSync(dir, { withFileTypes: true })) {
          const full = path.join(dir, ent.name);
          const sub = rel ? `${rel}/${ent.name}` : ent.name;
          if (ent.isDirectory()) walk(full, sub);
          else if (ent.isFile()) files[sub] = fs.readFileSync(full);
        }
      };
      walk(vfsRoot, "");
    }
    const wasmBytes = fs.readFileSync(wasmPath);
    const shim = new BrowserWasiShim({
      args: wasmArgs.length ? wasmArgs : [path.basename(wasmPath)],
      env: {},
      files,
      stdin: stdinData,
    });
    const res = await shim.run(wasmBytes);
    if (vfsRoot) {
      for (const [k, data] of res.vfs.entries()) {
        const outP = path.join(vfsRoot, k);
        fs.mkdirSync(path.dirname(outP), { recursive: true });
        fs.writeFileSync(outP, data);
      }
    }
    if (res.stdout) process.stdout.write(res.stdout);
    if (res.stderr) process.stderr.write(res.stderr);
    process.exit(res.exitCode);
  })().catch((err) => {
    console.error(err);
    process.exit(255);
  });
}
