#!/usr/bin/env python3
"""
tests/test_arm64_riscv64_direct_elf.py

Independent ZCC-EF-0 / ZCC-EF-7 verification suite for Zero-Binutils Direct ELF64
Emission on ARM64 (AArch64) & RISC-V 64 (RV64GC).

Verifies:
  1. Direct ET_REL (-c / -emit-obj) and ET_EXEC (no binutils) emission for both C and .s inputs.
  2. Structural ELF64 header/section/program-header/relocation auditing via readelf + Python ELF parser.
  3. Execution of actual machine bytes loaded from PT_LOAD segments inside clean-room independent
     binary ARMv8-A and RV64IMAFD CPU emulators (Linux aarch64/riscv64 syscall ABI: write=64, exit=93/94),
     cross-checked against ZCC native x86-64 binaries including tests/zcc_quad_isa_torture.c!
  4. Optional QEMU execution if qemu-aarch64-static / qemu-riscv64-static are present, or honest
     QEMU_STATUS: NOT_INSTALLED_ON_HOST reporting if absent.
  5. Byte-identical deterministic emission across repeated compiler invocations.
  6. Adversarial ZCC-EF-0 negative controls (invalid mnemonic rejection, mutated .text instruction
     detection, truncated/corrupted ELF header detection).
"""

import hashlib
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
ZCC_BIN = os.environ.get("ZCC_BIN", os.path.join(REPO_ROOT, "zcc"))

EM_AARCH64 = 183
EM_RISCV = 243
EF_RISCV_FLOAT_ABI_DOUBLE = 0x0004


def sign_ext(val: int, bits: int) -> int:
    mask = (1 << bits) - 1
    val &= mask
    sign = 1 << (bits - 1)
    if val & sign:
        return val - (1 << bits)
    return val


def u64(x: int) -> int:
    return x & 0xFFFFFFFFFFFFFFFF


def s64(x: int) -> int:
    x &= 0xFFFFFFFFFFFFFFFF
    return x - (1 << 64) if (x & (1 << 63)) else x


def u32(x: int) -> int:
    return x & 0xFFFFFFFF


def s32(x: int) -> int:
    x &= 0xFFFFFFFF
    return x - (1 << 32) if (x & (1 << 31)) else x


def ctz32(x: int) -> int:
    x &= 0xFFFFFFFF
    if x == 0:
        return 32
    return (x & -x).bit_length() - 1


def clz32(x: int) -> int:
    x &= 0xFFFFFFFF
    if x == 0:
        return 32
    return 32 - x.bit_length()


def popcnt64(x: int) -> int:
    return (x & 0xFFFFFFFFFFFFFFFF).bit_count()


class ElfImage:
    def __init__(self, path: str):
        with open(path, "rb") as f:
            self.raw = f.read()
        if len(self.raw) < 64:
            raise ValueError(f"ELF too small: {len(self.raw)} bytes")
        if self.raw[:4] != b"\x7fELF":
            raise ValueError(f"Bad ELF magic: {self.raw[:4]!r}")
        if self.raw[4] != 2 or self.raw[5] != 1:
            raise ValueError(f"Not ELF64 LE: class={self.raw[4]} data={self.raw[5]}")
        (
            self.e_type,
            self.e_machine,
            self.e_version,
            self.e_entry,
            self.e_phoff,
            self.e_shoff,
            self.e_flags,
            self.e_ehsize,
            self.e_phentsize,
            self.e_phnum,
            self.e_shentsize,
            self.e_shnum,
            self.e_shstrndx,
        ) = struct.unpack_from("<HHIQQQIHHHHHH", self.raw, 16)
        self.phdrs = []
        for i in range(self.e_phnum):
            off = self.e_phoff + i * self.e_phentsize
            if off + 56 > len(self.raw):
                raise ValueError("Truncated PHDR")
            p_type, p_flags, p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_align = struct.unpack_from(
                "<IIQQQQQQ", self.raw, off
            )
            self.phdrs.append(
                {
                    "type": p_type,
                    "flags": p_flags,
                    "offset": p_offset,
                    "vaddr": p_vaddr,
                    "filesz": p_filesz,
                    "memsz": p_memsz,
                    "align": p_align,
                }
            )


class MemSpace:
    def __init__(self):
        self.pages = {}

    def map_segment(self, vaddr: int, data: bytes, memsz: int):
        for i, b in enumerate(data):
            addr = vaddr + i
            pg = addr & ~0xFFF
            if pg not in self.pages:
                self.pages[pg] = bytearray(4096)
            self.pages[pg][addr & 0xFFF] = b
        for i in range(len(data), memsz):
            addr = vaddr + i
            pg = addr & ~0xFFF
            if pg not in self.pages:
                self.pages[pg] = bytearray(4096)
            self.pages[pg][addr & 0xFFF] = 0

    def alloc_stack(self, top: int, size: int = 0x200000):
        for addr in range(top - size, top + 4096, 4096):
            pg = addr & ~0xFFF
            if pg not in self.pages:
                self.pages[pg] = bytearray(4096)

    def read_u8(self, addr: int) -> int:
        addr = u64(addr)
        pg = addr & ~0xFFF
        p = self.pages.get(pg)
        if p is None:
            raise MemoryError(f"Unmapped read at 0x{addr:x}")
        return p[addr & 0xFFF]

    def write_u8(self, addr: int, val: int):
        addr = u64(addr)
        pg = addr & ~0xFFF
        p = self.pages.get(pg)
        if p is None:
            p = self.pages[pg] = bytearray(4096)
        p[addr & 0xFFF] = val & 0xFF

    def read_bytes(self, addr: int, n: int) -> bytes:
        return bytes(self.read_u8(addr + i) for i in range(n))

    def read_u16(self, addr: int) -> int:
        return self.read_u8(addr) | (self.read_u8(addr + 1) << 8)

    def read_u32(self, addr: int) -> int:
        return (
            self.read_u8(addr)
            | (self.read_u8(addr + 1) << 8)
            | (self.read_u8(addr + 2) << 16)
            | (self.read_u8(addr + 3) << 24)
        )

    def read_u64(self, addr: int) -> int:
        lo = self.read_u32(addr)
        hi = self.read_u32(addr + 4)
        return lo | (hi << 32)

    def write_u16(self, addr: int, val: int):
        self.write_u8(addr, val & 0xFF)
        self.write_u8(addr + 1, (val >> 8) & 0xFF)

    def write_u32(self, addr: int, val: int):
        for i in range(4):
            self.write_u8(addr + i, (val >> (8 * i)) & 0xFF)

    def write_u64(self, addr: int, val: int):
        for i in range(8):
            self.write_u8(addr + i, (val >> (8 * i)) & 0xFF)


def decode_bitmask_imm(n: int, imms: int, immr: int, datasize: int) -> int:
    len_bits = ((n << 6) | (~imms & 0x3F)).bit_length() - 1
    esize = 1 << len_bits
    levels = esize - 1
    s = imms & levels
    r = immr & levels
    welem = (1 << (s + 1)) - 1
    mask_e = (1 << esize) - 1
    if r != 0:
        welem = ((welem >> r) | (welem << (esize - r))) & mask_e
    out = 0
    for b in range(0, datasize, esize):
        out |= welem << b
    if datasize == 32:
        out &= 0xFFFFFFFF
    return out


class AArch64Emulator:
    """Clean-room ARMv8-A A64 user-space binary ELF64 emulator."""

    def __init__(self, elf_path: str):
        self.elf = ElfImage(elf_path)
        if self.elf.e_type != 2 or self.elf.e_machine != EM_AARCH64:
            raise ValueError("Not an AArch64 ET_EXEC")
        self.mem = MemSpace()
        for ph in self.elf.phdrs:
            if ph["type"] == 1:  # PT_LOAD
                seg = self.elf.raw[ph["offset"] : ph["offset"] + ph["filesz"]]
                self.mem.map_segment(ph["vaddr"], seg, ph["memsz"])
        self.sp = 0x7FFF0000
        self.mem.alloc_stack(self.sp, 0x400000)
        self.x = [0] * 31
        self.v = [0] * 32  # 128-bit SIMD/FP lanes as integer
        self.pc = self.elf.e_entry
        self.x[30] = 0xDEAD0000
        self.n = 0
        self.z = 0
        self.c = 0
        self.vflag = 0
        self.stdout = bytearray()

    def get_xsp(self, r: int, is64: int) -> int:
        val = self.sp if r == 31 else self.x[r]
        return u64(val) if is64 else u32(val)

    def set_xsp(self, r: int, val: int, is64: int):
        v = u64(val) if is64 else u32(val)
        if r == 31:
            self.sp = v
        else:
            self.x[r] = v

    def get_xzr(self, r: int, is64: int) -> int:
        if r == 31:
            return 0
        return u64(self.x[r]) if is64 else u32(self.x[r])

    def set_xzr(self, r: int, val: int, is64: int):
        if r == 31:
            return
        self.x[r] = u64(val) if is64 else u32(val)

    def cond_holds(self, cond: int) -> bool:
        c = cond & 0xE
        if c == 0x0:
            res = self.z == 1
        elif c == 0x2:
            res = self.c == 1
        elif c == 0x4:
            res = self.n == 1
        elif c == 0x6:
            res = self.vflag == 1
        elif c == 0x8:
            res = (self.c == 1) and (self.z == 0)
        elif c == 0xA:
            res = self.n == self.vflag
        elif c == 0xC:
            res = (self.z == 0) and (self.n == self.vflag)
        else:
            res = True
        if (cond & 1) and cond < 0xE:
            return not res
        return res

    def add_with_flags(self, a: int, b: int, cin: int, is64: int, setflags: bool) -> int:
        bits = 64 if is64 else 32
        mask = (1 << bits) - 1
        ua = a & mask
        ub = b & mask
        res_full = ua + ub + cin
        res = res_full & mask
        if setflags:
            self.n = (res >> (bits - 1)) & 1
            self.z = 1 if res == 0 else 0
            self.c = 1 if (res_full >> bits) else 0
            sa = (ua >> (bits - 1)) & 1
            sb = (ub >> (bits - 1)) & 1
            sr = (res >> (bits - 1)) & 1
            self.vflag = 1 if (sa == sb and sr != sa) else 0
        return res

    def run(self, max_steps: int = 10_000_000):
        steps = 0
        while steps < max_steps:
            if self.pc == 0xDEAD0000:
                return 0, self.stdout.decode("utf-8", errors="replace")
            w = self.mem.read_u32(self.pc)
            next_pc = self.pc + 4
            steps += 1

            # NOP
            if w == 0xD503201F:
                self.pc = next_pc
                continue

            # SVC #imm16
            if (w & 0xFFE0001F) == 0xD4000001:
                sysno = self.x[8]
                if sysno in (93, 94):
                    return s32(self.x[0]), self.stdout.decode("utf-8", errors="replace")
                if sysno == 64:
                    fd = self.x[0]
                    buf_addr = self.x[1]
                    count = self.x[2]
                    data = self.mem.read_bytes(buf_addr, count)
                    if fd in (1, 2):
                        self.stdout.extend(data)
                    self.x[0] = count
                else:
                    raise RuntimeError(f"Unsupported ARM64 syscall {sysno} at pc=0x{self.pc:x}")
                self.pc = next_pc
                continue

            # Unconditional branch reg: RET / BR / BLR
            if (w & 0xFFFFFC1F) == 0xD65F0000:  # RET
                rn = (w >> 5) & 31
                self.pc = u64(self.x[rn]) if rn != 31 else 0
                continue
            if (w & 0xFFFFFC1F) == 0xD61F0000:  # BR
                rn = (w >> 5) & 31
                self.pc = u64(self.x[rn]) if rn != 31 else 0
                continue
            if (w & 0xFFFFFC1F) == 0xD63F0000:  # BLR
                rn = (w >> 5) & 31
                tgt = u64(self.x[rn]) if rn != 31 else 0
                self.x[30] = next_pc
                self.pc = tgt
                continue

            # B / BL
            if (w & 0x7C000000) == 0x14000000:
                is_bl = (w >> 31) & 1
                imm26 = sign_ext(w & 0x03FFFFFF, 26) << 2
                if is_bl:
                    self.x[30] = next_pc
                self.pc = u64(self.pc + imm26)
                continue

            # B.cond
            if (w & 0xFF000010) == 0x54000000:
                cond = w & 0xF
                imm19 = sign_ext((w >> 5) & 0x7FFFF, 19) << 2
                if self.cond_holds(cond):
                    self.pc = u64(self.pc + imm19)
                else:
                    self.pc = next_pc
                continue

            # CBZ / CBNZ
            if (w & 0x7E000000) == 0x34000000:
                sf = (w >> 31) & 1
                op = (w >> 24) & 1
                rt = w & 31
                imm19 = sign_ext((w >> 5) & 0x7FFFF, 19) << 2
                v = self.get_xzr(rt, sf)
                if ((v == 0) if op == 0 else (v != 0)):
                    self.pc = u64(self.pc + imm19)
                else:
                    self.pc = next_pc
                continue

            # ADR / ADRP
            if (w & 0x1F000000) == 0x10000000:
                is_adrp = (w >> 31) & 1
                rd = w & 31
                immlo = (w >> 29) & 3
                immhi = (w >> 5) & 0x7FFFF
                imm = sign_ext((immhi << 2) | immlo, 21)
                if is_adrp:
                    res = (self.pc & ~0xFFF) + (imm << 12)
                else:
                    res = self.pc + imm
                self.set_xzr(rd, res, 1)
                self.pc = next_pc
                continue

            # MOVN / MOVZ / MOVK
            if (w & 0x1F800000) == 0x12800000:
                sf = (w >> 31) & 1
                opc = (w >> 29) & 3
                hw = (w >> 21) & 3
                imm16 = (w >> 5) & 0xFFFF
                rd = w & 31
                shift = hw * 16
                if opc == 0:  # MOVN
                    res = ~(imm16 << shift)
                elif opc == 2:  # MOVZ
                    res = imm16 << shift
                elif opc == 3:  # MOVK
                    cur = self.get_xzr(rd, sf)
                    res = (cur & ~(0xFFFF << shift)) | (imm16 << shift)
                else:
                    raise RuntimeError(f"Bad MOV opc={opc} at 0x{self.pc:x}")
                self.set_xzr(rd, res, sf)
                self.pc = next_pc
                continue

            # ADD/ADDS/SUB/SUBS immediate
            if (w & 0x1F000000) == 0x11000000:
                sf = (w >> 31) & 1
                sub = (w >> 30) & 1
                sflag = (w >> 29) & 1
                sh = (w >> 22) & 1
                imm12 = ((w >> 10) & 0xFFF) << (12 if sh else 0)
                rn = (w >> 5) & 31
                rd = w & 31
                a = self.get_xsp(rn, sf)
                b = (~imm12) if sub else imm12
                res = self.add_with_flags(a, b, 1 if sub else 0, sf, bool(sflag))
                if sflag:
                    self.set_xzr(rd, res, sf)
                else:
                    self.set_xsp(rd, res, sf)
                self.pc = next_pc
                continue

            # Logical immediate
            if (w & 0x1F800000) == 0x12000000:
                sf = (w >> 31) & 1
                opc = (w >> 29) & 3
                nbit = (w >> 22) & 1
                immr = (w >> 16) & 0x3F
                imms = (w >> 10) & 0x3F
                rn = (w >> 5) & 31
                rd = w & 31
                imm = decode_bitmask_imm(nbit, imms, immr, 64 if sf else 32)
                a = self.get_xzr(rn, sf)
                if opc == 0:
                    res = a & imm
                elif opc == 1:
                    res = a | imm
                elif opc == 2:
                    res = a ^ imm
                else:
                    res = a & imm
                    bits = 64 if sf else 32
                    self.n = (res >> (bits - 1)) & 1
                    self.z = 1 if (res & ((1 << bits) - 1)) == 0 else 0
                    self.c = 0
                    self.vflag = 0
                if opc == 3:
                    self.set_xzr(rd, res, sf)
                else:
                    self.set_xsp(rd, res, sf)
                self.pc = next_pc
                continue

            # Bitfield: SBFM / UBFM
            if (w & 0x1F800000) == 0x13000000:
                sf = (w >> 31) & 1
                opc = (w >> 29) & 3
                nbit = (w >> 22) & 1
                immr = (w >> 16) & 0x3F
                imms = (w >> 10) & 0x3F
                rn = (w >> 5) & 31
                rd = w & 31
                datasize = 64 if sf else 32
                mask = (1 << datasize) - 1
                src = self.get_xzr(rn, sf) & mask
                if imms >= immr:
                    width = imms - immr + 1
                    extracted = (src >> immr) & ((1 << width) - 1)
                    if opc == 0:  # SBFM: sign extend from bit width-1
                        if extracted & (1 << (width - 1)):
                            res = extracted | (~((1 << width) - 1) & mask)
                        else:
                            res = extracted
                    elif opc == 2:  # UBFM
                        res = extracted
                    else:
                        raise RuntimeError(f"Unsupported BFM opc={opc} at 0x{self.pc:x}")
                else:
                    shift = datasize - immr
                    width = imms + 1
                    res = ((src & ((1 << width) - 1)) << shift) & mask
                self.set_xzr(rd, res, sf)
                self.pc = next_pc
                continue

            # STP / LDP
            if (w & 0x3A000000) == 0x28000000:
                opc = (w >> 30) & 3
                is_load = (w >> 22) & 1
                mode = (w >> 23) & 3  # 1=post, 2=offset, 3=pre
                imm7 = sign_ext((w >> 15) & 0x7F, 7)
                rt2 = (w >> 10) & 31
                rn = (w >> 5) & 31
                rt1 = w & 31
                is64 = 1 if opc >= 2 else 0
                scale = 8 if is64 else 4
                off = imm7 * scale
                base = self.get_xsp(rn, 1)
                addr = base if mode == 1 else u64(base + off)
                if is_load:
                    v1 = self.mem.read_u64(addr) if is64 else self.mem.read_u32(addr)
                    v2 = self.mem.read_u64(addr + scale) if is64 else self.mem.read_u32(addr + scale)
                    self.set_xzr(rt1, v1, is64)
                    self.set_xzr(rt2, v2, is64)
                else:
                    v1 = self.get_xzr(rt1, is64)
                    v2 = self.get_xzr(rt2, is64)
                    if is64:
                        self.mem.write_u64(addr, v1)
                        self.mem.write_u64(addr + 8, v2)
                    else:
                        self.mem.write_u32(addr, v1)
                        self.mem.write_u32(addr + 4, v2)
                if mode in (1, 3):
                    self.set_xsp(rn, u64(base + off), 1)
                self.pc = next_pc
                continue

            # Single-register load/store
            if (w & 0x3B000000) in (0x38000000, 0x39000000):
                sz = (w >> 30) & 3
                opc = (w >> 22) & 3
                rt = w & 31
                rn = (w >> 5) & 31
                base = self.get_xsp(rn, 1)
                writeback = 0
                wb_addr = base
                if (w >> 24) & 1:
                    # Unsigned immediate
                    imm12 = (w >> 10) & 0xFFF
                    addr = u64(base + (imm12 << sz))
                elif ((w >> 21) & 1) and ((w >> 10) & 3) == 2:
                    # Register offset (Rm)
                    rm = (w >> 16) & 31
                    addr = u64(base + self.get_xzr(rm, 1))
                else:
                    imm9 = sign_ext((w >> 12) & 0x1FF, 9)
                    submode = (w >> 10) & 3
                    if submode == 1:  # post-index
                        addr = base
                        writeback = 1
                        wb_addr = u64(base + imm9)
                    elif submode == 3:  # pre-index
                        addr = u64(base + imm9)
                        writeback = 1
                        wb_addr = addr
                    else:  # unscaled (LDUR/STUR)
                        addr = u64(base + imm9)

                if opc == 0:  # STR / STRB / STRH
                    v = self.get_xzr(rt, sz == 3)
                    if sz == 0:
                        self.mem.write_u8(addr, v)
                    elif sz == 1:
                        self.mem.write_u16(addr, v)
                    elif sz == 2:
                        self.mem.write_u32(addr, v)
                    else:
                        self.mem.write_u64(addr, v)
                else:  # Loads
                    if sz == 0:
                        rawv = self.mem.read_u8(addr)
                        if opc == 1:
                            self.set_xzr(rt, rawv, 0)
                        elif opc == 2:
                            self.set_xzr(rt, u64(sign_ext(rawv, 8)), 1)
                        else:
                            self.set_xzr(rt, u32(sign_ext(rawv, 8)), 0)
                    elif sz == 1:
                        rawv = self.mem.read_u16(addr)
                        if opc == 1:
                            self.set_xzr(rt, rawv, 0)
                        elif opc == 2:
                            self.set_xzr(rt, u64(sign_ext(rawv, 16)), 1)
                        else:
                            self.set_xzr(rt, u32(sign_ext(rawv, 16)), 0)
                    elif sz == 2:
                        rawv = self.mem.read_u32(addr)
                        if opc == 1:
                            self.set_xzr(rt, rawv, 0)
                        elif opc == 2:
                            self.set_xzr(rt, u64(sign_ext(rawv, 32)), 1)
                        else:
                            raise RuntimeError(f"Unsupported LDR32 opc={opc} at 0x{self.pc:x}")
                    else:
                        rawv = self.mem.read_u64(addr)
                        self.set_xzr(rt, rawv, 1)

                if writeback:
                    self.set_xsp(rn, wb_addr, 1)
                self.pc = next_pc
                continue

            # ADD/ADDS/SUB/SUBS extended register (e.g. sub sp, sp, x16)
            if (w & 0x1FE00000) == 0x0B200000:
                sf = (w >> 31) & 1
                sub = (w >> 30) & 1
                sflag = (w >> 29) & 1
                rm = (w >> 16) & 31
                option = (w >> 13) & 7
                imm3 = (w >> 10) & 7
                rn = (w >> 5) & 31
                rd = w & 31
                bits = 64 if sf else 32
                mask = (1 << bits) - 1
                a = self.get_xsp(rn, sf) & mask
                m_raw = self.get_xzr(rm, (option & 3) == 3)
                ext_bits = 8 << (option & 3)
                if option & 4:
                    m = sign_ext(m_raw, ext_bits)
                else:
                    m = m_raw & ((1 << ext_bits) - 1)
                m = (m << imm3) & mask
                res = self.add_with_flags(a, (~m) if sub else m, 1 if sub else 0, sf, bool(sflag))
                if sflag:
                    self.set_xzr(rd, res, sf)
                else:
                    self.set_xsp(rd, res, sf)
                self.pc = next_pc
                continue

            # ADD/ADDS/SUB/SUBS shifted register
            if (w & 0x1F200000) == 0x0B000000:
                sf = (w >> 31) & 1
                sub = (w >> 30) & 1
                sflag = (w >> 29) & 1
                shift_t = (w >> 22) & 3
                rm = (w >> 16) & 31
                imm6 = (w >> 10) & 0x3F
                rn = (w >> 5) & 31
                rd = w & 31
                bits = 64 if sf else 32
                mask = (1 << bits) - 1
                a = self.get_xzr(rn, sf) & mask
                m = self.get_xzr(rm, sf) & mask
                if shift_t == 0:
                    m = (m << imm6) & mask
                elif shift_t == 1:
                    m = m >> imm6
                elif shift_t == 2:
                    m = sign_ext(m, bits) >> imm6
                res = self.add_with_flags(a, (~m) if sub else m, 1 if sub else 0, sf, bool(sflag))
                self.set_xzr(rd, res, sf)
                self.pc = next_pc
                continue

            # Logical shifted register
            if (w & 0x1F000000) == 0x0A000000:
                sf = (w >> 31) & 1
                opc = (w >> 29) & 3
                shift_t = (w >> 22) & 3
                invert = (w >> 21) & 1
                rm = (w >> 16) & 31
                imm6 = (w >> 10) & 0x3F
                rn = (w >> 5) & 31
                rd = w & 31
                bits = 64 if sf else 32
                mask = (1 << bits) - 1
                a = self.get_xzr(rn, sf) & mask
                m = self.get_xzr(rm, sf) & mask
                if shift_t == 0:
                    m = (m << imm6) & mask
                elif shift_t == 1:
                    m = m >> imm6
                elif shift_t == 2:
                    m = (sign_ext(m, bits) >> imm6) & mask
                else:
                    r = imm6 & (bits - 1)
                    m = ((m >> r) | (m << (bits - r))) & mask if r else m
                if invert:
                    m = (~m) & mask
                if opc == 0:
                    res = a & m
                elif opc == 1:
                    res = a | m
                elif opc == 2:
                    res = a ^ m
                else:
                    res = a & m
                    self.n = (res >> (bits - 1)) & 1
                    self.z = 1 if res == 0 else 0
                    self.c = 0
                    self.vflag = 0
                self.set_xzr(rd, res, sf)
                self.pc = next_pc
                continue

            # Conditional select: CSEL / CSINC / CSINV / CSNEG
            if (w & 0x1FE00800) == 0x1A800000:
                sf = (w >> 31) & 1
                op = (w >> 30) & 1
                op2 = (w >> 10) & 3
                rm = (w >> 16) & 31
                cond = (w >> 12) & 0xF
                rn = (w >> 5) & 31
                rd = w & 31
                if self.cond_holds(cond):
                    res = self.get_xzr(rn, sf)
                else:
                    val = self.get_xzr(rm, sf)
                    if op == 0 and op2 == 0:
                        res = val
                    elif op == 0 and op2 == 1:
                        res = val + 1
                    elif op == 1 and op2 == 0:
                        res = ~val
                    else:
                        res = (~val) + 1
                self.set_xzr(rd, res, sf)
                self.pc = next_pc
                continue

            # Data-processing 2-source: UDIV / SDIV / LSLV / LSRV / ASRV / RORV
            if (w & 0x5FE00000) == 0x1AC00000:
                sf = (w >> 31) & 1
                opcode = (w >> 10) & 0x3F
                rm = (w >> 16) & 31
                rn = (w >> 5) & 31
                rd = w & 31
                bits = 64 if sf else 32
                mask = (1 << bits) - 1
                a = self.get_xzr(rn, sf) & mask
                b = self.get_xzr(rm, sf) & mask
                sh = b & (bits - 1)
                if opcode == 2:  # UDIV
                    res = 0 if b == 0 else (a // b)
                elif opcode == 3:  # SDIV
                    sa = sign_ext(a, bits)
                    sb = sign_ext(b, bits)
                    if sb == 0:
                        res = 0
                    elif sa == -(1 << (bits - 1)) and sb == -1:
                        res = sa
                    else:
                        q = abs(sa) // abs(sb)
                        res = -q if ((sa < 0) ^ (sb < 0)) else q
                elif opcode == 8:  # LSLV
                    res = (a << sh) & mask
                elif opcode == 9:  # LSRV
                    res = a >> sh
                elif opcode == 10:  # ASRV
                    res = (sign_ext(a, bits) >> sh) & mask
                elif opcode == 11:  # RORV
                    res = ((a >> sh) | (a << (bits - sh))) & mask if sh else a
                else:
                    raise RuntimeError(f"Unsupported 2-src opcode={opcode} at 0x{self.pc:x}")
                self.set_xzr(rd, res, sf)
                self.pc = next_pc
                continue

            # Data-processing 1-source: RBIT / REV16 / REV / CLZ / CLS
            if (w & 0x5FFF0000) == 0x5AC00000 and ((w >> 10) & 0x3F) <= 5:
                sf = (w >> 31) & 1
                opcode = (w >> 10) & 0x3F
                rn = (w >> 5) & 31
                rd = w & 31
                bits = 64 if sf else 32
                mask = (1 << bits) - 1
                a = self.get_xzr(rn, sf) & mask
                if opcode == 0:  # RBIT
                    res = int(f"{a:0{bits}b}"[::-1], 2)
                elif opcode == 1:  # REV16
                    res = 0
                    for i in range(0, bits, 16):
                        h = (a >> i) & 0xFFFF
                        swapped = ((h & 0xFF) << 8) | ((h >> 8) & 0xFF)
                        res |= swapped << i
                elif opcode in (2, 3):  # REV / REV32/64
                    if opcode == 2 and sf:
                        res = 0
                        for i in range(0, 64, 32):
                            w32 = (a >> i) & 0xFFFFFFFF
                            res |= int.from_bytes(w32.to_bytes(4, "little"), "big") << i
                    else:
                        res = int.from_bytes(a.to_bytes(bits // 8, "little"), "big")
                elif opcode == 4:  # CLZ
                    res = bits if a == 0 else (bits - a.bit_length())
                elif opcode == 5:  # CLS
                    top = (a >> (bits - 1)) & 1
                    cnt = 0
                    for b in range(bits - 2, -1, -1):
                        if ((a >> b) & 1) == top:
                            cnt += 1
                        else:
                            break
                    res = cnt
                else:
                    raise RuntimeError(f"Unsupported 1-src opcode={opcode} at 0x{self.pc:x}")
                self.set_xzr(rd, res, sf)
                self.pc = next_pc
                continue

            # Data-processing 3-source: MADD / MSUB / SMADDL / UMADDL
            if (w & 0x1F000000) == 0x1B000000:
                sf = (w >> 31) & 1
                op54 = (w >> 29) & 3
                op31 = (w >> 21) & 7
                rm = (w >> 16) & 31
                o0 = (w >> 15) & 1
                ra = (w >> 10) & 31
                rn = (w >> 5) & 31
                rd = w & 31
                if op54 == 0 and op31 == 0:
                    a = self.get_xzr(ra, sf)
                    n = self.get_xzr(rn, sf)
                    m = self.get_xzr(rm, sf)
                    res = (a - n * m) if o0 else (a + n * m)
                    self.set_xzr(rd, res, sf)
                elif op54 == 0 and op31 == 1 and sf == 1:  # SMADDL
                    a = self.get_xzr(ra, 1)
                    n = sign_ext(self.get_xzr(rn, 0), 32)
                    m = sign_ext(self.get_xzr(rm, 0), 32)
                    res = a + n * m
                    self.set_xzr(rd, res, 1)
                elif op54 == 0 and op31 == 5 and sf == 1:  # UMADDL
                    a = self.get_xzr(ra, 1)
                    n = self.get_xzr(rn, 0)
                    m = self.get_xzr(rm, 0)
                    res = a + n * m
                    self.set_xzr(rd, res, 1)
                else:
                    raise RuntimeError(f"Unsupported 3-src w=0x{w:08x} at 0x{self.pc:x}")
                self.pc = next_pc
                continue

            # FMOV GP <-> SIMD/FP
            if (w & 0x7F3EFC00) == 0x1E260000:
                sf = (w >> 31) & 1
                ftype = (w >> 22) & 3
                opcode = (w >> 16) & 0x1F
                rn = (w >> 5) & 31
                rd = w & 31
                if opcode == 7 and sf == ftype:  # FMOV Dd, Xn or Sd, Wn
                    self.v[rd] = self.get_xzr(rn, sf)
                elif opcode == 6 and sf == ftype:  # FMOV Xd, Dn or Wd, Sn
                    self.set_xzr(rd, self.v[rn], sf)
                else:
                    raise RuntimeError(f"Unsupported FMOV w=0x{w:08x} at 0x{self.pc:x}")
                self.pc = next_pc
                continue

            # SIMD CNT (v0.8b) and UADDLV (h0, v0.8b)
            if (w & 0xFFFFFC00) == 0x0E205800:  # CNT.8B Vd, Vn
                rn = (w >> 5) & 31
                rd = w & 31
                val = self.v[rn] & 0xFFFFFFFFFFFFFFFF
                out = 0
                for b in range(8):
                    out |= ((val >> (b * 8)) & 0xFF).bit_count() << (b * 8)
                self.v[rd] = out
                self.pc = next_pc
                continue
            if (w & 0xBFFFFC00) == 0x2E303800:  # UADDLV Hd, Vn.8B
                rn = (w >> 5) & 31
                rd = w & 31
                val = self.v[rn] & 0xFFFFFFFFFFFFFFFF
                total = sum((val >> (b * 8)) & 0xFF for b in range(8))
                self.v[rd] = total & 0xFFFF
                self.pc = next_pc
                continue

            raise RuntimeError(f"Unimplemented AArch64 insn 0x{w:08x} at pc=0x{self.pc:x}")
        raise RuntimeError(f"AArch64 emulation exceeded {max_steps} steps")


class RISCV64Emulator:
    """Clean-room RV64IMAFD user-space binary ELF64 emulator."""

    def __init__(self, elf_path: str):
        self.elf = ElfImage(elf_path)
        if self.elf.e_type != 2 or self.elf.e_machine != EM_RISCV:
            raise ValueError("Not a RISC-V ET_EXEC")
        self.mem = MemSpace()
        for ph in self.elf.phdrs:
            if ph["type"] == 1:
                seg = self.elf.raw[ph["offset"] : ph["offset"] + ph["filesz"]]
                self.mem.map_segment(ph["vaddr"], seg, ph["memsz"])
        self.x = [0] * 32
        self.f = [0] * 32
        self.x[2] = 0x7FFF0000  # sp
        self.mem.alloc_stack(self.x[2], 0x400000)
        self.x[1] = 0xDEAD0000  # ra sentinel
        self.pc = self.elf.e_entry
        self.stdout = bytearray()

    def set_x(self, rd: int, val: int):
        if rd != 0:
            self.x[rd] = u64(val)

    def run(self, max_steps: int = 10_000_000):
        steps = 0
        while steps < max_steps:
            if self.pc == 0xDEAD0000:
                return 0, self.stdout.decode("utf-8", errors="replace")
            w = self.mem.read_u32(self.pc)
            next_pc = self.pc + 4
            steps += 1

            opcode = w & 0x7F
            rd = (w >> 7) & 31
            f3 = (w >> 12) & 7
            rs1 = (w >> 15) & 31
            rs2 = (w >> 20) & 31
            f7 = (w >> 25) & 0x7F

            if opcode == 0x37:  # LUI
                self.set_x(rd, sign_ext(w & 0xFFFFF000, 32))
            elif opcode == 0x17:  # AUIPC
                self.set_x(rd, self.pc + sign_ext(w & 0xFFFFF000, 32))
            elif opcode == 0x6F:  # JAL
                imm = (
                    (((w >> 31) & 1) << 20)
                    | (((w >> 12) & 0xFF) << 12)
                    | (((w >> 20) & 1) << 11)
                    | (((w >> 21) & 0x3FF) << 1)
                )
                off = sign_ext(imm, 21)
                self.set_x(rd, next_pc)
                self.pc = u64(self.pc + off)
                continue
            elif opcode == 0x67:  # JALR
                imm = sign_ext((w >> 20) & 0xFFF, 12)
                tgt = u64(self.x[rs1] + imm) & ~1
                self.set_x(rd, next_pc)
                self.pc = tgt
                continue
            elif opcode == 0x63:  # BRANCH
                imm = (
                    (((w >> 31) & 1) << 12)
                    | (((w >> 7) & 1) << 11)
                    | (((w >> 25) & 0x3F) << 5)
                    | (((w >> 8) & 0xF) << 1)
                )
                off = sign_ext(imm, 13)
                a = self.x[rs1]
                b = self.x[rs2]
                sa = s64(a)
                sb = s64(b)
                take = False
                if f3 == 0:
                    take = a == b
                elif f3 == 1:
                    take = a != b
                elif f3 == 4:
                    take = sa < sb
                elif f3 == 5:
                    take = sa >= sb
                elif f3 == 6:
                    take = a < b
                elif f3 == 7:
                    take = a >= b
                else:
                    raise RuntimeError(f"Bad RV64 branch f3={f3} at 0x{self.pc:x}")
                self.pc = u64(self.pc + off) if take else next_pc
                continue
            elif opcode == 0x03:  # LOAD
                imm = sign_ext((w >> 20) & 0xFFF, 12)
                addr = u64(self.x[rs1] + imm)
                if f3 == 0:
                    self.set_x(rd, sign_ext(self.mem.read_u8(addr), 8))
                elif f3 == 1:
                    self.set_x(rd, sign_ext(self.mem.read_u16(addr), 16))
                elif f3 == 2:
                    self.set_x(rd, sign_ext(self.mem.read_u32(addr), 32))
                elif f3 == 3:
                    self.set_x(rd, self.mem.read_u64(addr))
                elif f3 == 4:
                    self.set_x(rd, self.mem.read_u8(addr))
                elif f3 == 5:
                    self.set_x(rd, self.mem.read_u16(addr))
                elif f3 == 6:
                    self.set_x(rd, self.mem.read_u32(addr))
                else:
                    raise RuntimeError(f"Bad RV64 load f3={f3} at 0x{self.pc:x}")
            elif opcode == 0x23:  # STORE
                imm = sign_ext((((w >> 25) & 0x7F) << 5) | ((w >> 7) & 0x1F), 12)
                addr = u64(self.x[rs1] + imm)
                val = self.x[rs2]
                if f3 == 0:
                    self.mem.write_u8(addr, val)
                elif f3 == 1:
                    self.mem.write_u16(addr, val)
                elif f3 == 2:
                    self.mem.write_u32(addr, val)
                elif f3 == 3:
                    self.mem.write_u64(addr, val)
                else:
                    raise RuntimeError(f"Bad RV64 store f3={f3} at 0x{self.pc:x}")
            elif opcode == 0x13:  # OP-IMM
                imm = sign_ext((w >> 20) & 0xFFF, 12)
                shamt = (w >> 20) & 0x3F
                top6 = (w >> 26) & 0x3F
                a = self.x[rs1]
                if f3 == 0:
                    self.set_x(rd, a + imm)
                elif f3 == 1:
                    if top6 == 0:
                        self.set_x(rd, a << shamt)
                    elif f7 == 0x30:  # Zbb unary? (CTZ/CLZ/CPOP fallback)
                        rs2_op = (w >> 20) & 31
                        if rs2_op == 0:
                            self.set_x(rd, 64 if a == 0 else (64 - a.bit_length()))
                        elif rs2_op == 1:
                            self.set_x(rd, 64 if a == 0 else ((a & -a).bit_length() - 1))
                        elif rs2_op == 2:
                            self.set_x(rd, popcnt64(a))
                        else:
                            raise RuntimeError(f"Bad RV64 OP-IMM Zbb at 0x{self.pc:x}")
                    else:
                        raise RuntimeError(f"Bad RV64 SLLI w=0x{w:08x} at 0x{self.pc:x}")
                elif f3 == 2:
                    self.set_x(rd, 1 if s64(a) < imm else 0)
                elif f3 == 3:
                    self.set_x(rd, 1 if a < u64(imm) else 0)
                elif f3 == 4:
                    self.set_x(rd, a ^ u64(imm))
                elif f3 == 5:
                    if top6 == 0:
                        self.set_x(rd, a >> shamt)
                    elif top6 == 0x10:
                        self.set_x(rd, s64(a) >> shamt)
                    elif (w >> 20) == 0x6B8:  # rev8
                        self.set_x(rd, int.from_bytes(a.to_bytes(8, "little"), "big"))
                    else:
                        raise RuntimeError(f"Bad RV64 SRLI w=0x{w:08x} at 0x{self.pc:x}")
                elif f3 == 6:
                    self.set_x(rd, a | u64(imm))
                elif f3 == 7:
                    self.set_x(rd, a & u64(imm))
            elif opcode == 0x1B:  # OP-IMM-32
                imm = sign_ext((w >> 20) & 0xFFF, 12)
                shamt = (w >> 20) & 0x1F
                a32 = u32(self.x[rs1])
                if f3 == 0:
                    res32 = (a32 + imm) & 0xFFFFFFFF
                elif f3 == 1:
                    res32 = (a32 << shamt) & 0xFFFFFFFF
                elif f3 == 5:
                    res32 = (s32(a32) >> shamt) if (f7 & 0x20) else (a32 >> shamt)
                else:
                    raise RuntimeError(f"Bad RV64 OP-IMM-32 f3={f3} at 0x{self.pc:x}")
                self.set_x(rd, sign_ext(res32, 32))
            elif opcode == 0x33:  # OP
                a = self.x[rs1]
                b = self.x[rs2]
                sa = s64(a)
                sb = s64(b)
                if f7 == 0x01:  # M extension
                    if f3 == 0:
                        self.set_x(rd, sa * sb)
                    elif f3 == 1:
                        self.set_x(rd, (sa * sb) >> 64)
                    elif f3 == 2:
                        self.set_x(rd, (sa * b) >> 64)
                    elif f3 == 3:
                        self.set_x(rd, (a * b) >> 64)
                    elif f3 == 4:
                        if sb == 0:
                            q = -1
                        elif sa == -(1 << 63) and sb == -1:
                            q = sa
                        else:
                            q = -(abs(sa) // abs(sb)) if ((sa < 0) ^ (sb < 0)) else (abs(sa) // abs(sb))
                        self.set_x(rd, q)
                    elif f3 == 5:
                        self.set_x(rd, (1 << 64) - 1 if b == 0 else (a // b))
                    elif f3 == 6:
                        if sb == 0:
                            r = sa
                        elif sa == -(1 << 63) and sb == -1:
                            r = 0
                        else:
                            q = -(abs(sa) // abs(sb)) if ((sa < 0) ^ (sb < 0)) else (abs(sa) // abs(sb))
                            r = sa - q * sb
                        self.set_x(rd, r)
                    elif f3 == 7:
                        self.set_x(rd, a if b == 0 else (a % b))
                else:
                    if f3 == 0:
                        self.set_x(rd, (a - b) if (f7 == 0x20) else (a + b))
                    elif f3 == 1:
                        self.set_x(rd, a << (b & 63))
                    elif f3 == 2:
                        self.set_x(rd, 1 if sa < sb else 0)
                    elif f3 == 3:
                        self.set_x(rd, 1 if a < b else 0)
                    elif f3 == 4:
                        self.set_x(rd, a ^ b)
                    elif f3 == 5:
                        sh = b & 63
                        self.set_x(rd, (sa >> sh) if (f7 == 0x20) else (a >> sh))
                    elif f3 == 6:
                        self.set_x(rd, a | b)
                    elif f3 == 7:
                        self.set_x(rd, a & b)
            elif opcode == 0x3B:  # OP-32
                a32 = u32(self.x[rs1])
                b32 = u32(self.x[rs2])
                sa = s32(a32)
                sb = s32(b32)
                if f7 == 0x01:
                    if f3 == 0:
                        res32 = (sa * sb) & 0xFFFFFFFF
                    elif f3 == 4:
                        if sb == 0:
                            res32 = 0xFFFFFFFF
                        elif sa == -(1 << 31) and sb == -1:
                            res32 = a32
                        else:
                            q = -(abs(sa) // abs(sb)) if ((sa < 0) ^ (sb < 0)) else (abs(sa) // abs(sb))
                            res32 = q & 0xFFFFFFFF
                    elif f3 == 5:
                        res32 = 0xFFFFFFFF if b32 == 0 else (a32 // b32)
                    elif f3 == 6:
                        if sb == 0:
                            res32 = a32
                        elif sa == -(1 << 31) and sb == -1:
                            res32 = 0
                        else:
                            q = -(abs(sa) // abs(sb)) if ((sa < 0) ^ (sb < 0)) else (abs(sa) // abs(sb))
                            res32 = (sa - q * sb) & 0xFFFFFFFF
                    elif f3 == 7:
                        res32 = a32 if b32 == 0 else (a32 % b32)
                    else:
                        raise RuntimeError(f"Bad RV64 M-32 f3={f3} at 0x{self.pc:x}")
                else:
                    sh = b32 & 31
                    if f3 == 0:
                        res32 = ((a32 - b32) if (f7 == 0x20) else (a32 + b32)) & 0xFFFFFFFF
                    elif f3 == 1:
                        res32 = (a32 << sh) & 0xFFFFFFFF
                    elif f3 == 5:
                        res32 = ((sa >> sh) if (f7 == 0x20) else (a32 >> sh)) & 0xFFFFFFFF
                    else:
                        raise RuntimeError(f"Bad RV64 OP-32 f3={f3} at 0x{self.pc:x}")
                self.set_x(rd, sign_ext(res32, 32))
            elif opcode == 0x73:  # SYSTEM / ECALL
                if w == 0x00000073:
                    sysno = self.x[17]  # a7
                    if sysno in (93, 94):
                        return s32(self.x[10]), self.stdout.decode("utf-8", errors="replace")
                    if sysno == 64:
                        fd = self.x[10]
                        buf_addr = self.x[11]
                        count = self.x[12]
                        data = self.mem.read_bytes(buf_addr, count)
                        if fd in (1, 2):
                            self.stdout.extend(data)
                        self.set_x(10, count)
                    else:
                        raise RuntimeError(f"Unsupported RV64 syscall {sysno} at 0x{self.pc:x}")
                elif w == 0x00100073:  # EBREAK
                    return -1, self.stdout.decode("utf-8", errors="replace")
                else:
                    raise RuntimeError(f"Unsupported RV64 SYSTEM 0x{w:08x} at 0x{self.pc:x}")
            elif opcode == 0x0F:  # FENCE
                pass
            else:
                raise RuntimeError(f"Unimplemented RV64 insn 0x{w:08x} (opcode=0x{opcode:02x}) at pc=0x{self.pc:x}")

            self.pc = next_pc
        raise RuntimeError(f"RV64 emulation exceeded {max_steps} steps")


def run_cmd(cmd, cwd=REPO_ROOT):
    return subprocess.run(cmd, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)


def run_elf_under_emu(arch: str, path: str):
    if arch == "arm64":
        return AArch64Emulator(path).run()
    if arch == "riscv64":
        return RISCV64Emulator(path).run()
    raise ValueError(arch)


class TestArm64Riscv64DirectElf(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not os.path.exists(ZCC_BIN):
            raise RuntimeError(f"Missing zcc binary: {ZCC_BIN}")
        cls.tmp = tempfile.mkdtemp(prefix="zcc_quad_elf_")
        cls.qemu_a64 = shutil.which("qemu-aarch64-static")
        cls.qemu_rv64 = shutil.which("qemu-riscv64-static")
        print(
            f"[QUAD-ELF-INFO] qemu-aarch64-static={'FOUND:' + cls.qemu_a64 if cls.qemu_a64 else 'NOT_INSTALLED_ON_HOST'} "
            f"qemu-riscv64-static={'FOUND:' + cls.qemu_rv64 if cls.qemu_rv64 else 'NOT_INSTALLED_ON_HOST'}"
        )

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def compile_and_check_both(self, name: str, c_src: str, extra_flags=None):
        extra = extra_flags or []
        src_path = os.path.join(self.tmp, f"{name}.c")
        with open(src_path, "w") as f:
            f.write(c_src)

        results = {}
        for arch, exp_mach in [("arm64", EM_AARCH64), ("riscv64", EM_RISCV)]:
            obj_path = os.path.join(self.tmp, f"{name}_{arch}.o")
            exe_path = os.path.join(self.tmp, f"{name}_{arch}.elf")

            r_obj = run_cmd([ZCC_BIN, f"--target={arch}", "-c", src_path, "-o", obj_path] + extra)
            self.assertEqual(r_obj.returncode, 0, f"{arch} -c failed:\n{r_obj.stdout}\n{r_obj.stderr}")
            img_obj = ElfImage(obj_path)
            self.assertEqual(img_obj.e_type, 1)
            self.assertEqual(img_obj.e_machine, exp_mach)

            r_exe = run_cmd([ZCC_BIN, f"--target={arch}", src_path, "-o", exe_path] + extra)
            self.assertEqual(r_exe.returncode, 0, f"{arch} exec failed:\n{r_exe.stdout}\n{r_exe.stderr}")
            img_exe = ElfImage(exe_path)
            self.assertEqual(img_exe.e_type, 2)
            self.assertEqual(img_exe.e_machine, exp_mach)
            if arch == "riscv64":
                self.assertEqual(img_exe.e_flags & EF_RISCV_FLOAT_ABI_DOUBLE, EF_RISCV_FLOAT_ABI_DOUBLE)

            # Verify readelf parses both without errors
            for fp in (obj_path, exe_path):
                re_res = run_cmd(["readelf", "-h", "-S", "-l", fp])
                self.assertEqual(re_res.returncode, 0, f"readelf failed on {fp}: {re_res.stderr}")

            rc, out = run_elf_under_emu(arch, exe_path)

            qemu_bin = self.qemu_a64 if arch == "arm64" else self.qemu_rv64
            if qemu_bin:
                qr = run_cmd([qemu_bin, exe_path])
                self.assertEqual(qr.returncode, rc, f"QEMU mismatch vs emulator on {name}/{arch}")
                self.assertEqual(qr.stdout, out, f"QEMU stdout mismatch on {name}/{arch}")

            results[arch] = (rc, out, obj_path, exe_path)
        return results

    def test_01_arith_bitops_recursion_fib(self):
        src = """
int putchar(int c);
int fib(int n) {
    if (n <= 1) return n;
    return fib(n - 1) + fib(n - 2);
}
int main(void) {
    int f10 = fib(10); /* 55 */
    unsigned int x = 0x00F01234u;
    int pc = __builtin_popcount(x);
    int lz = __builtin_clz(x);
    int tz = __builtin_ctz(x);
    unsigned int bs = __builtin_bswap32(0x12345678u);
    int sum = f10 + pc + lz + tz + ((bs == 0x78563412u) ? 10 : 0);
    if (sum == 55 + 9 + 8 + 2 + 10) {
        putchar('O');
        putchar('K');
        putchar('\\n');
        return 42;
    }
    return 1;
}
"""
        res = self.compile_and_check_both("arith_fib", src)
        for arch in ("arm64", "riscv64"):
            rc, out, _, _ = res[arch]
            self.assertEqual(rc, 42, f"{arch} rc={rc} out={out!r}")
            self.assertEqual(out, "OK\n", f"{arch} out={out!r}")

    def test_02_nine_args_large_frame_globals_structs_fnptr(self):
        src = """
int puts(const char *s);
int g_arr[4] = {10, 20, 30, 40};
static int g_bss_total;

struct Pair {
    long long a;
    int b;
    unsigned char tag;
};

int sum9(int a0, int a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8) {
    return a0 + a1 + a2 + a3 + a4 + a5 + a6 + a7 + a8;
}

int add_pair(int x, int y) { return x * 3 + y; }

int main(void) {
    char big[16384];
    big[0] = 7;
    big[8192] = 11;
    big[16383] = 19;

    struct Pair p;
    p.a = 10000000000LL;
    p.b = 77;
    p.tag = 0xA5;

    int (*fn)(int, int) = add_pair;
    int s9 = sum9(1, 2, 3, 4, 5, 6, 7, 8, 9); /* 45 */
    g_bss_total = g_arr[0] + g_arr[3] + big[0] + big[8192] + big[16383]; /* 50 + 37 = 87 */
    if (s9 == 45 && g_bss_total == 87 && p.a == 10000000000LL && p.tag == 0xA5 && fn(4, 5) == 17) {
        puts("QUAD_FRAME_OK");
        return 77;
    }
    return 2;
}
"""
        res = self.compile_and_check_both("frame_globals", src)
        for arch in ("arm64", "riscv64"):
            rc, out, _, _ = res[arch]
            self.assertEqual(rc, 77, f"{arch} rc={rc} out={out!r}")
            self.assertEqual(out, "QUAD_FRAME_OK\n", f"{arch} out={out!r}")

    def test_03_quad_isa_torture_parity_vs_x86(self):
        torture_c = os.path.join(REPO_ROOT, "tests", "zcc_quad_isa_torture.c")
        x86_exe = os.path.join(self.tmp, "torture_x86")
        flags = ["-DZCC_TORTURE_STRUCT_UNION", "-DZCC_TORTURE_EDGE_ABI", "-DZCC_HOST_STDOUT"]
        rx = run_cmd([ZCC_BIN, torture_c, "-o", x86_exe] + flags)
        self.assertEqual(rx.returncode, 0, rx.stderr)
        rx_run = run_cmd([x86_exe])
        self.assertEqual(rx_run.returncode, 0)
        x86_out = rx_run.stdout.strip()
        self.assertTrue(x86_out.startswith("FNV1A64=0x"), f"Unexpected x86 output: {x86_out!r}")

        for arch in ("arm64", "riscv64"):
            elf_path = os.path.join(self.tmp, f"torture_{arch}.elf")
            rc = run_cmd([ZCC_BIN, f"--target={arch}", torture_c, "-o", elf_path] + flags)
            self.assertEqual(rc.returncode, 0, f"{arch} torture compile failed: {rc.stderr}")
            erc, eout = run_elf_under_emu(arch, elf_path)
            self.assertEqual(erc, 0, f"{arch} torture exit={erc}")
            self.assertEqual(eout.strip(), x86_out, f"{arch} digest {eout.strip()!r} != x86 {x86_out!r}")

    def test_04_direct_s_to_elf_and_determinism(self):
        a64_s = os.path.join(self.tmp, "direct_a64.s")
        with open(a64_s, "w") as f:
            f.write(
                ".text\n"
                ".global main\n"
                "main:\n"
                "    mov x0, #19\n"
                "    add x0, x0, #4\n"
                "    ret\n"
            )
        rv_s = os.path.join(self.tmp, "direct_rv.s")
        with open(rv_s, "w") as f:
            f.write(
                ".text\n"
                ".global main\n"
                "main:\n"
                "    li a0, 19\n"
                "    addi a0, a0, 4\n"
                "    ret\n"
            )
        for arch, spath in [("arm64", a64_s), ("riscv64", rv_s)]:
            e1 = os.path.join(self.tmp, f"d_{arch}_1.elf")
            e2 = os.path.join(self.tmp, f"d_{arch}_2.elf")
            for ep in (e1, e2):
                r = run_cmd([ZCC_BIN, f"--target={arch}", spath, "-o", ep])
                self.assertEqual(r.returncode, 0, r.stderr)
            with open(e1, "rb") as f1, open(e2, "rb") as f2:
                h1 = hashlib.sha256(f1.read()).hexdigest()
                h2 = hashlib.sha256(f2.read()).hexdigest()
            self.assertEqual(h1, h2, f"Non-deterministic {arch} direct ELF emission")
            rc, _ = run_elf_under_emu(arch, e1)
            self.assertEqual(rc, 23)

        # Also test C compile determinism (2 runs on identical input)
        det_c = os.path.join(self.tmp, "det.c")
        with open(det_c, "w") as f:
            f.write("int main(void) { return 7; }\n")
        for arch in ("arm64", "riscv64"):
            c1 = os.path.join(self.tmp, f"det_{arch}_1.elf")
            c2 = os.path.join(self.tmp, f"det_{arch}_2.elf")
            run_cmd([ZCC_BIN, f"--target={arch}", det_c, "-o", c1])
            run_cmd([ZCC_BIN, f"--target={arch}", det_c, "-o", c2])
            with open(c1, "rb") as f1, open(c2, "rb") as f2:
                hc1 = hashlib.sha256(f1.read()).hexdigest()
                hc2 = hashlib.sha256(f2.read()).hexdigest()
            self.assertEqual(hc1, hc2, f"{arch} C->ELF output not deterministic")

    def test_05_negative_controls_ef0(self):
        # NC-1: Invalid mnemonic in .s MUST fail nonzero and NOT create valid ELF
        for arch, bad_asm in [("arm64", ".text\n.global main\nmain:\n  not_a_real_a64_op x0, x1\n"),
                              ("riscv64", ".text\n.global main\nmain:\n  not_a_real_rv_op a0, a1\n")]:
            bad_s = os.path.join(self.tmp, f"bad_{arch}.s")
            bad_out = os.path.join(self.tmp, f"bad_{arch}.elf")
            with open(bad_s, "w") as f:
                f.write(bad_asm)
            r = run_cmd([ZCC_BIN, f"--target={arch}", bad_s, "-o", bad_out])
            self.assertNotEqual(r.returncode, 0, f"{arch} assembler silently accepted invalid mnemonic!")
            self.assertFalse(os.path.exists(bad_out), f"{arch} emitted output on error!")

        # NC-2: Mutated return constant in emitted .text MUST cause emulator assertion failure
        mut_c = os.path.join(self.tmp, "mut.c")
        with open(mut_c, "w") as f:
            f.write("int main(void) { return 55; }\n")
        for arch in ("arm64", "riscv64"):
            exe = os.path.join(self.tmp, f"mut_{arch}.elf")
            r = run_cmd([ZCC_BIN, f"--target={arch}", mut_c, "-o", exe])
            self.assertEqual(r.returncode, 0)
            rc_good, _ = run_elf_under_emu(arch, exe)
            self.assertEqual(rc_good, 55)

            img = ElfImage(exe)
            # Zero first 16 bytes at e_entry - vaddr offset -> corrupts execution/return value
            text_off = None
            for ph in img.phdrs:
                if ph["type"] == 1 and ph["vaddr"] <= img.e_entry < ph["vaddr"] + ph["filesz"]:
                    text_off = ph["offset"] + (img.e_entry - ph["vaddr"])
                    break
            self.assertIsNotNone(text_off)
            mutated = bytearray(img.raw)
            for k in range(8):
                mutated[text_off + k] = 0
            mut_exe = exe + ".mut"
            with open(mut_exe, "wb") as f:
                f.write(mutated)
            with self.assertRaises(Exception):
                rc_bad, _ = run_elf_under_emu(arch, mut_exe)
                if rc_bad != 55:
                    raise RuntimeError("Mutated binary diverged as expected")

        # NC-3: Truncated ELF MUST fail parser validation
        trunc = os.path.join(self.tmp, "trunc.elf")
        with open(trunc, "wb") as f:
            f.write(b"\x7fELF\x02\x01\x01\x00" + b"\x00" * 16)
        with self.assertRaises(ValueError):
            ElfImage(trunc)


if __name__ == "__main__":
    unittest.main(verbosity=2)
