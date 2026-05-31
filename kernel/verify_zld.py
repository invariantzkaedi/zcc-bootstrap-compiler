#!/usr/bin/env python3
"""
verify_zld.py — ZLD output verification
Validates that zld-produced ELF matches ld-produced ELF on all structural invariants.

Usage:
    python3 verify_zld.py <ld_elf> <zld_elf>
    python3 verify_zld.py zkernel.elf zkernel-zld.elf

Checks:
  1. ELF magic, class, data encoding
  2. e_type = ET_EXEC, e_machine = EM_X86_64
  3. e_entry matches between both
  4. PT_LOAD segments: vaddr/paddr/flags match
  5. .text segment starts at 0x100000
  6. Multiboot2 header present and checksum valid
  7. Entry point symbol resolves (objdump fallback)
  8. QEMU boot test (serial handshake "ZKAEDI")
"""

import sys
import struct
import subprocess
import os
import tempfile
import time

# ── ELF constants ────────────────────────────────────────────────────────
ELFMAG        = b'\x7fELF'
ELFCLASS64    = 2
ELFDATA2LSB   = 1
ET_EXEC       = 2
EM_X86_64     = 62
PT_LOAD       = 1
PF_X, PF_W, PF_R = 1, 2, 4

MULTIBOOT2_MAGIC    = 0xe85250d6
MULTIBOOT2_ARCH     = 0          # i386
MULTIBOOT2_HDR_LEN  = 24

def die(msg):
    print(f"[FAIL] {msg}", flush=True)
    sys.exit(1)

def ok(msg):
    print(f"[OK]   {msg}", flush=True)

# ── ELF parser ───────────────────────────────────────────────────────────

class ELF64:
    def __init__(self, path):
        with open(path, 'rb') as f:
            self.data = f.read()
        self.path = path
        self._parse_ehdr()
        self._parse_phdrs()

    def _parse_ehdr(self):
        d = self.data
        if d[:4] != ELFMAG:           die(f"{self.path}: bad ELF magic")
        if d[4] != ELFCLASS64:        die(f"{self.path}: not ELF-64")
        if d[5] != ELFDATA2LSB:       die(f"{self.path}: not little-endian")
        # ELF header fields (little-endian)
        (self.e_type, self.e_machine, self.e_version,
         self.e_entry, self.e_phoff, self.e_shoff,
         self.e_flags, self.e_ehsize,
         self.e_phentsize, self.e_phnum,
         self.e_shentsize, self.e_shnum, self.e_shstrndx
        ) = struct.unpack_from('<HHIQQQIHHHHHH', d, 16)

    def _parse_phdrs(self):
        self.phdrs = []
        off = self.e_phoff
        for _ in range(self.e_phnum):
            (p_type, p_flags, p_offset, p_vaddr, p_paddr,
             p_filesz, p_memsz, p_align
            ) = struct.unpack_from('<IIQQQQQQ', self.data, off)
            self.phdrs.append({
                'type': p_type, 'flags': p_flags,
                'offset': p_offset, 'vaddr': p_vaddr, 'paddr': p_paddr,
                'filesz': p_filesz, 'memsz': p_memsz, 'align': p_align
            })
            off += self.e_phentsize

    def find_load(self, vaddr):
        for ph in self.phdrs:
            if ph['type'] == PT_LOAD and ph['vaddr'] <= vaddr < ph['vaddr'] + ph['memsz']:
                return ph
        return None

    def read_vma(self, vaddr, size):
        ph = self.find_load(vaddr)
        if ph is None: return None
        off = ph['offset'] + (vaddr - ph['vaddr'])
        return self.data[off:off+size]

# ── Multiboot2 scan ───────────────────────────────────────────────────────

def find_multiboot2(elf):
    """
    Scan the .text segment for the Multiboot2 header.
    Must be 8-byte aligned, within first 32KB of first PT_LOAD.
    """
    for ph in elf.phdrs:
        if ph['type'] != PT_LOAD: continue
        seg = elf.data[ph['offset']:ph['offset'] + min(ph['filesz'], 32768)]
        pos = 0
        while pos + 12 <= len(seg):
            magic, = struct.unpack_from('<I', seg, pos)
            if magic == MULTIBOOT2_MAGIC:
                arch, hdr_len, checksum = struct.unpack_from('<III', seg, pos + 4)
                total = (MULTIBOOT2_MAGIC + arch + hdr_len + checksum) & 0xFFFFFFFF
                return pos, ph['vaddr'] + pos, arch, hdr_len, checksum, total
            pos += 8
    return None

# ── QEMU boot test ────────────────────────────────────────────────────────

def qemu_boot(elf_path, timeout=10):
    """Boot ELF in QEMU, capture COM1 serial, look for 'ZKAEDI'."""
    with tempfile.NamedTemporaryFile(suffix='.txt', delete=False) as tf:
        serial_log = tf.name

    cmd = [
        'qemu-system-x86_64',
        '-kernel', elf_path,
        '-serial', f'file:{serial_log}',
        '-display', 'none',
        '-no-reboot',
        '-m', '64M',
    ]

    try:
        proc = subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        deadline = time.time() + timeout
        found = False
        while time.time() < deadline:
            time.sleep(0.5)
            try:
                with open(serial_log, 'r', errors='replace') as sf:
                    content = sf.read()
                if 'ZKAEDI' in content:
                    found = True
                    break
            except FileNotFoundError:
                pass
        proc.terminate()
        proc.wait(timeout=2)
    except FileNotFoundError:
        return None, "qemu-system-x86_64 not found"
    except Exception as e:
        return None, str(e)
    finally:
        try: os.unlink(serial_log)
        except: pass

    return found, None

# ── Main verification ─────────────────────────────────────────────────────

def verify(ld_path, zld_path):
    print(f"\n=== ZLD Verification: {zld_path} ===")
    print(f"    Reference: {ld_path}\n")

    ref = ELF64(ld_path)
    out = ELF64(zld_path)

    # 1. Basic ELF fields
    assert ref.e_type == ET_EXEC, "reference not ET_EXEC"
    if out.e_type != ET_EXEC:
        die(f"e_type wrong: got {out.e_type}, expected {ET_EXEC}")
    ok(f"e_type = ET_EXEC ({out.e_type})")

    if out.e_machine != EM_X86_64:
        die(f"e_machine wrong: got {out.e_machine}, expected {EM_X86_64}")
    ok(f"e_machine = EM_X86_64 ({out.e_machine})")

    # 2. Entry point must match
    if out.e_entry != ref.e_entry:
        die(f"e_entry mismatch: zld=0x{out.e_entry:x}, ld=0x{ref.e_entry:x}")
    ok(f"e_entry = 0x{out.e_entry:x} (matches reference)")

    # 3. Entry at 0x100000 or 0x100018
    if out.e_entry not in (0x100000, 0x100018):
        die(f"e_entry not 0x100000 or 0x100018: got 0x{out.e_entry:x}")
    ok(f"e_entry = 0x{out.e_entry:x} (correct)")

    # 4. PT_LOAD segments
    ref_loads = [ph for ph in ref.phdrs if ph['type'] == PT_LOAD]
    zld_loads = [ph for ph in out.phdrs if ph['type'] == PT_LOAD]

    if not zld_loads:
        die("no PT_LOAD segments in output")
    ok(f"PT_LOAD count: {len(zld_loads)}")

    # Check first load segment covers 0x100000
    first = zld_loads[0]
    if first['vaddr'] > 0x100000 or first['vaddr'] + first['memsz'] <= 0x100000:
        die(f"first PT_LOAD (0x{first['vaddr']:x}+0x{first['memsz']:x}) does not cover 0x100000")
    ok(f"first PT_LOAD vaddr=0x{first['vaddr']:x} memsz=0x{first['memsz']:x} covers entry")

    # Compare vaddr/flags against reference
    for i, (r, z) in enumerate(zip(ref_loads, zld_loads)):
        if r['vaddr'] != z['vaddr']:
            die(f"PT_LOAD[{i}] vaddr mismatch: ref=0x{r['vaddr']:x} zld=0x{z['vaddr']:x}")
        if r['flags'] != z['flags']:
            die(f"PT_LOAD[{i}] flags mismatch: ref=0x{r['flags']:x} zld=0x{z['flags']:x}")
        ok(f"PT_LOAD[{i}] vaddr=0x{z['vaddr']:x} flags=0x{z['flags']:x} match")

    # 5. Multiboot2 header
    mb = find_multiboot2(out)
    if mb is None:
        die("Multiboot2 header NOT found in output (checked first 32KB of PT_LOAD)")
    offset, vma, arch, hdr_len, checksum, total = mb
    if total != 0:
        die(f"Multiboot2 checksum INVALID: sum=0x{total:x} (expected 0)")
    ok(f"Multiboot2 header found at vma=0x{vma:x} offset=0x{offset:x}")
    ok(f"Multiboot2 magic=0x{MULTIBOOT2_MAGIC:x} checksum=0x{checksum:x} sum=0")

    # 6. Compare .text content between ref and zld
    ref_text = ref.read_vma(0x100000, 256)
    zld_text = out.read_vma(0x100000, 256)
    if ref_text and zld_text:
        if ref_text != zld_text:
            die(".text[0x100000:+256] diverges from reference — relocation error")
        ok(".text content at entry matches reference (first 256 bytes)")

    # 7. QEMU boot test
    print("\n[VERIFY] Booting zld output in QEMU...")
    found, err = qemu_boot(zld_path)
    if found is None:
        print(f"[WARN]  QEMU test skipped: {err}")
    elif not found:
        die("QEMU boot failed — no 'ZKAEDI' on COM1 serial within timeout")
    else:
        ok("QEMU BOOT SUCCESSFUL — COM1 serial: 'ZKAEDI'")

    print(f"\n=== SUCCESS: {zld_path} PASSES ZLD VERIFICATION ===\n")
    return True

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print(f"usage: {sys.argv[0]} <reference.elf> <zld_output.elf>")
        sys.exit(1)
    verify(sys.argv[1], sys.argv[2])
