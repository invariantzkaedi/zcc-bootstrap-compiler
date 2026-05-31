import sys

def patch_elf(filename):
    print(f"Patching {filename}...")
    with open(filename, "r+b") as f:
        f.seek(18)
        f.write(b"\x03\x00")
    print("Patch applied: e_machine changed to EM_386.")

if __name__ == "__main__":
    patch_elf("zkernel.elf")
