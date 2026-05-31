# Forensic Ticket: ZCC Struct Packing Alignment Defect & Stack-Array Workaround (FORENSIC_025)

## 1. Goal
Document the structural layout compiler bug where ZCC ignores packed attributes on structures containing 64-bit primitive fields (such as `unsigned long`), resulting in implicit padding that breaks hardware/register boundary specifications.

## 2. Pathology & Forensic Analysis
During bare-metal x86-64 interrupt descriptor table (IDT) configuration in `kernel/kmain.c`, `struct idt_ptr_64` was declared with `__attribute__((packed))` to load a 10-byte target descriptor for `lidt`:

```c
struct idt_ptr_64 {
    unsigned short limit;   // 2 bytes
    unsigned long  base;    // 8 bytes
} __attribute__((packed));
```

Under GNU GCC/Clang, `sizeof(struct idt_ptr_64)` is exactly `10` bytes, and the offset of `base` is exactly `2` bytes.

However, in ZCC Stage-2 and Stage-3, the compiler's structure member layout
allocator (defined in `part3.c`) fails to apply structural packing for `>=8`-byte
boundaries. Instead, ZCC aligns `unsigned long` on a strict 8-byte boundary,
placing `base` at offset 8 (yielding sizeof = 16 bytes) with 6 bytes of padding
between `limit` and `base`.

When the CPU's native `lidt` loaded this un-packed structure, the 10 bytes it
read were: limit at bytes 0..1, then padding at bytes 2..7, then only bytes
8..9 of `base` — so the base register was loaded almost entirely from
uninitialized stack padding, never reaching the real IDT address. This
produced a garbage base (e.g. 0x3000000000000000), triggering a General
Protection Fault (check_exception old: 0xffffffff new 0xd), then a Double
Fault and Triple Fault loop on the first timer interrupt.

---

## Status
RESOLVED — root cause fixed in part3.c (recompute_struct_layout), commit 142b783d.
Kernel migrated off the idt_p[10] workaround in 72a90cd4.

## Workaround
kernel/kmain.c builds the IDT descriptor as a manual 10-byte stack array (unsigned char idt_p[10]) instead of relying on struct idt_ptr_64 with __attribute__((packed)). Zero runtime overhead; does not address the compiler defect.

## Root cause (unfixed)
ZCC's layout allocator ignores __attribute__((packed)) when a struct contains a >=8-byte primitive (e.g. unsigned long), forcing 8-byte alignment and inserting padding. Any packed struct crossing a hardware or ABI boundary (descriptor tables, MMIO maps, on-wire/on-disk formats) is affected. Fix requires teaching the layout allocator to honor packed on >=8-byte fields.
