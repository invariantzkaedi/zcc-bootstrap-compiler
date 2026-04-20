import sys
import re

def parse_telemetry(log_file):
    telem = {}
    with open(log_file, "r") as f:
        for line in f:
            # Example: [telem] fn=trace used_regs=0x1F src=forced
            m = re.search(r"\[telem\] fn=(\S+) used_regs=(0x[0-9a-fA-F]+) src=(\S+)", line)
            if m:
                fn = m.group(1)
                used_regs = int(m.group(2), 16)
                src = m.group(3)
                telem[fn] = {"used_regs": used_regs, "src": src}
    return telem

def parse_asm(asm_file):
    asm_usage = {}
    current_fn = None
    mask = 0
    with open(asm_file, "r") as f:
        for line in f:
            line = line.strip()
            if line.endswith(":") and not line.startswith("."):
                if current_fn:
                    asm_usage[current_fn] = mask
                current_fn = line[:-1]
                mask = 0
            elif current_fn:
                # Find all xmm registers
                for m in re.finditer(r"%xmm(\d+)", line):
                    reg_num = int(m.group(1))
                    mask |= (1 << reg_num)
    if current_fn:
        asm_usage[current_fn] = mask
    return asm_usage

def main():
    if len(sys.argv) < 3:
        print("Usage: python asm_scraper.py <telemetry.log> <file.s>")
        sys.exit(1)

    log_file = sys.argv[1]
    asm_file = sys.argv[2]

    telem = parse_telemetry(log_file)
    asm_usage = parse_asm(asm_file)

    print(f"{'Function':<20} | {'Telem (Used)':<15} | {'Actual (Ref)':<15} | {'Slack':<10} | {'Src'}")
    print("-" * 75)

    all_valid = True
    for fn, data in telem.items():
        t_mask = data["used_regs"]
        src = data["src"]
        a_mask = asm_usage.get(fn, 0)
        
        # Slack is what the compiler allocated but the assembly didn't use.
        slack = t_mask & ~a_mask
        # Over-usage: the assembly used a register that wasn't allocated/reported (this would be a fatal bug!)
        fatal = a_mask & ~t_mask

        print(f"{fn:<20} | 0x{t_mask:02X}            | 0x{a_mask:02X}            | 0x{slack:02X}       | {src}")

        if fatal > 0:
            print(f"FATAL ERROR: Function '{fn}' actually referenced xmm registers (0x{fatal:02X}) NOT reported in telemetry!")
            all_valid = False

    if all_valid:
        print("\nAll functions valid. No under-reporting of register usage found.")
    else:
        print("\nVALIDATION FAILED.")
        sys.exit(1)

if __name__ == '__main__':
    main()
