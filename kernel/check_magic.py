with open("zkernel.elf", "rb") as f:
    data = f.read(16384)
    mb1 = data.find(b"\x02\xb0\xad\x1b") # 0x1badb002 in little endian
    mb2 = data.find(b"\xd6\x50\x52\xe8") # 0xe85250d6 in little endian
    print(f"Multiboot1 magic offset: {mb1}")
    print(f"Multiboot2 magic offset: {mb2}")
