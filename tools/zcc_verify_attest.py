import sys
import os
import struct
import hashlib
import json

def get_dtype_name(dtype):
    mapping = {
        0: "F32",
        1: "F16",
        2: "Q4_0",
        3: "Q4_1",
        6: "Q5_0",
        7: "Q5_1",
        8: "Q8_0"
    }
    return mapping.get(dtype, f"UNKNOWN_{dtype}")

def compute_nbytes(dtype, shape):
    total_elements = 1
    for dim in shape:
        total_elements *= dim
        
    if dtype == 0:  # F32
        return total_elements * 4
    elif dtype == 1:  # F16
        return total_elements * 2
    elif dtype == 2:  # Q4_0
        return ((total_elements + 31) // 32) * 18
    elif dtype == 3:  # Q4_1
        return ((total_elements + 31) // 32) * 20
    elif dtype == 6:  # Q5_0
        return ((total_elements + 31) // 32) * 22
    elif dtype == 7:  # Q5_1
        return ((total_elements + 31) // 32) * 24
    elif dtype == 8:  # Q8_0
        return ((total_elements + 31) // 32) * 34
    else:
        raise ValueError(f"Unknown GGUF dtype {dtype}")

def read_string(f):
    len_bytes = f.read(8)
    if len(len_bytes) < 8:
        raise EOFError("Unexpected EOF reading string length")
    length = struct.unpack("<Q", len_bytes)[0]
    data = f.read(length)
    if len(data) < length:
        raise EOFError("Unexpected EOF reading string data")
    return data.decode("utf-8", errors="ignore")

def extract_attestation_section(elf_path):
    with open(elf_path, "rb") as f:
        # Read ELF64 Header
        f.seek(0)
        ehdr_data = f.read(64)
        if len(ehdr_data) < 64:
            return None, "Truncated ELF header"
        # Check Magic
        if ehdr_data[:4] != b'\x7fELF':
            return None, "Invalid ELF magic"
        
        # Parse shoff, shnum, shstrndx
        shoff, = struct.unpack("<Q", ehdr_data[40:48])
        shnum, = struct.unpack("<H", ehdr_data[60:62])
        shstrndx, = struct.unpack("<H", ehdr_data[62:64])
        
        # Read Section Headers
        f.seek(shoff)
        shdrs_data = f.read(shnum * 64)
        if len(shdrs_data) < shnum * 64:
            return None, "Truncated Section Header Table"
            
        shdrs = []
        for i in range(shnum):
            shdr = shdrs_data[i*64 : (i+1)*64]
            sh_name, sh_type, sh_flags, sh_addr, sh_offset, sh_size = struct.unpack("<IIQQQQ", shdr[:40])
            shdrs.append({
                "sh_name": sh_name,
                "sh_type": sh_type,
                "sh_flags": sh_flags,
                "sh_addr": sh_addr,
                "sh_offset": sh_offset,
                "sh_size": sh_size
            })
            
        # Read shstrtab to resolve section names
        if shstrndx >= shnum:
            return None, "Invalid shstrndx value"
        strtab_sh = shdrs[shstrndx]
        f.seek(strtab_sh["sh_offset"])
        strtab = f.read(strtab_sh["sh_size"])
        
        def get_name(offset):
            end = strtab.find(b'\x00', offset)
            if end != -1:
                return strtab[offset:end].decode('utf-8')
            return strtab[offset:].decode('utf-8')
            
        for sh in shdrs:
            name = get_name(sh["sh_name"])
            if name == ".zcc_tensor_attest":
                f.seek(sh["sh_offset"])
                return f.read(sh["sh_size"]), None
                
    return None, ".zcc_tensor_attest section not found in ELF"

def main():
    if len(sys.argv) < 3:
        print("Usage: python3 zcc_verify_attest.py <elf_executable> <gguf_file>")
        sys.exit(1)

    elf_path = sys.argv[1]
    gguf_path = sys.argv[2]

    # 1. Extract .zcc_tensor_attest from ELF
    attest_data, err = extract_attestation_section(elf_path)
    if err:
        print(f"ZCC_TENSOR_ERR_MISSING: {err}")
        sys.exit(1)

    if len(attest_data) < 128:
        print("ZCC_TENSOR_ERR_LAYOUT_NONCANONICAL: Attestation header too small")
        sys.exit(1)

    # 2. Parse Attestation Header
    magic, schema_version, verifier_version, gguf_version, flags, record_count, manifest_sha256, gguf_sha256, records_offset, records_size, reserved = struct.unpack(
        "<QIIIII32s32sQQ20s",
        attest_data[:128]
    )

    if magic != 0x5453415f43435a:
        print(f"ZCC_TENSOR_ERR_LAYOUT_NONCANONICAL: Invalid magic 0x{magic:016x}")
        sys.exit(1)

    if schema_version != 1:
        print(f"ZCC_TENSOR_ERR_LAYOUT_NONCANONICAL: Unsupported schema version {schema_version}")
        sys.exit(1)

    if verifier_version != 1:
        print(f"ZCC_TENSOR_ERR_LAYOUT_NONCANONICAL: Unsupported verifier version {verifier_version}")
        sys.exit(1)

    print(f"=== ELF Attestation Found: {elf_path} ===")
    print(f"  Record Count: {record_count}")
    print(f"  Schema Version: {schema_version}")
    print(f"  Verifier Version: {verifier_version}")
    print(f"  Expected GGUF Version: {gguf_version}")
    print(f"  Flags: {flags}")
    print(f"  Manifest SHA-256: {manifest_sha256.hex()}")
    print(f"  Expected GGUF SHA-256: {gguf_sha256.hex()}")

    # 3. Parse Records
    records = []
    for i in range(record_count):
        off = records_offset + i * 256
        if off + 256 > len(attest_data):
            print("ZCC_TENSOR_ERR_LAYOUT_NONCANONICAL: Truncated record array")
            sys.exit(1)
        
        record_bytes = attest_data[off : off+256]
        name_raw, dtype, rank, s0, s1, s2, s3, offset, nbytes, alignment, flags, t_sha256, l_sha256 = struct.unpack(
            "<128sII4QQQII32s32s",
            record_bytes
        )
        name = name_raw.decode('utf-8').rstrip('\x00')
        shape = [s0, s1, s2, s3][:rank]
        records.append({
            "name": name,
            "dtype": dtype,
            "rank": rank,
            "shape": shape,
            "offset": offset,
            "nbytes": nbytes,
            "alignment": alignment,
            "flags": flags,
            "tensor_sha256": t_sha256,
            "layout_sha256": l_sha256
        })

    # 4. Read GGUF and Verify Hash
    if not os.path.exists(gguf_path):
        print(f"ZCC_TENSOR_ERR_MISSING: GGUF file {gguf_path} not found")
        sys.exit(1)

    # Compute actual GGUF file hash
    sha256_full = hashlib.sha256()
    with open(gguf_path, "rb") as f:
        while True:
            chunk = f.read(1024 * 1024)
            if not chunk:
                break
            sha256_full.update(chunk)
    actual_gguf_sha256 = sha256_full.digest()

    if actual_gguf_sha256 != gguf_sha256:
        print("ZCC_TENSOR_ERR_SHA256_MISMATCH: GGUF file hash mismatch!")
        print(f"  Expected: {gguf_sha256.hex()}")
        print(f"  Got:      {actual_gguf_sha256.hex()}")
        sys.exit(1)

    print("✅ GGUF cryptographic file signature matches ELF attestation.")

    # 5. Verify individual tensors
    with open(gguf_path, "rb") as f:
        # Read header to find data offsets
        header_data = f.read(24)
        magic, version, gguf_tensor_count, metadata_kv_count = struct.unpack("<IIQQ", header_data)
        
        if version != gguf_version:
            print("ZCC_TENSOR_ERR_LAYOUT_NONCANONICAL: GGUF version mismatch!")
            print(f"  Expected (Attested): {gguf_version}")
            print(f"  Got (GGUF file):     {version}")
            sys.exit(1)
        
        # Skip metadata key-values
        for _ in range(metadata_kv_count):
            read_string(f)
            val_type = struct.unpack("<I", f.read(4))[0]
            if val_type in [0, 1]: f.read(1)
            elif val_type in [2, 3]: f.read(2)
            elif val_type in [4, 5, 6, 7]: f.read(4)
            elif val_type in [10, 11, 12]: f.read(8)
            elif val_type == 8: read_string(f)
            elif val_type == 9:
                elem_type = struct.unpack("<I", f.read(4))[0]
                count = struct.unpack("<Q", f.read(8))[0]
                for _ in range(count):
                    if elem_type in [0, 1]: f.read(1)
                    elif elem_type in [2, 3]: f.read(2)
                    elif elem_type in [4, 5, 6, 7]: f.read(4)
                    elif elem_type in [10, 11, 12]: f.read(8)
                    elif elem_type == 8: read_string(f)
            
        # Parse GGUF tensor infos
        gguf_tensors = {}
        for _ in range(gguf_tensor_count):
            name = read_string(f)
            n_dims = struct.unpack("<I", f.read(4))[0]
            dims = list(struct.unpack(f"<{n_dims}Q", f.read(8 * n_dims)))
            t_type = struct.unpack("<I", f.read(4))[0]
            offset = struct.unpack("<Q", f.read(8))[0]
            gguf_tensors[name] = {
                "dims": dims,
                "type": t_type,
                "offset": offset
            }

        metadata_end = f.read(0)  # current offset
        metadata_end = f.tell()
        tensor_data_start = (metadata_end + 31) & ~31

        # Compare records against GGUF
        for r in records:
            name = r["name"]
            if name not in gguf_tensors:
                print(f"ZCC_TENSOR_ERR_MISSING: Tensor '{name}' missing in GGUF")
                sys.exit(1)

            g_info = gguf_tensors[name]
            
            # Shape check
            if g_info["dims"] != r["shape"]:
                print(f"ZCC_TENSOR_ERR_SHAPE_MISMATCH: Tensor '{name}' shape mismatch!")
                print(f"  Attested: {r['shape']}")
                print(f"  GGUF:     {g_info['dims']}")
                sys.exit(1)

            # DType check
            if g_info["type"] != r["dtype"]:
                print(f"ZCC_TENSOR_ERR_DTYPE_MISMATCH: Tensor '{name}' dtype mismatch!")
                print(f"  Attested: {get_dtype_name(r['dtype'])}")
                print(f"  GGUF:     {get_dtype_name(g_info['type'])}")
                sys.exit(1)

            # Offset check
            if g_info["offset"] != r["offset"]:
                print(f"ZCC_TENSOR_ERR_OFFSET_MISMATCH: Tensor '{name}' offset mismatch!")
                print(f"  Attested: {r['offset']}")
                print(f"  GGUF:     {g_info['offset']}")
                sys.exit(1)

            # Payload validation
            t_offset = tensor_data_start + r["offset"]
            f.seek(t_offset)
            payload = f.read(r["nbytes"])
            if len(payload) < r["nbytes"]:
                print(f"ZCC_TENSOR_ERR_SHA256_MISMATCH: Truncated payload read for tensor '{name}'")
                sys.exit(1)

            actual_sha256 = hashlib.sha256(payload).digest()
            if actual_sha256 != r["tensor_sha256"]:
                print(f"ZCC_TENSOR_ERR_SHA256_MISMATCH: Tensor '{name}' payload hash mismatch!")
                print(f"  Attested: {r['tensor_sha256'].hex()}")
                print(f"  GGUF:     {actual_sha256.hex()}")
                sys.exit(1)

            # Recompute layout hash to confirm integrity
            shape_str = ",".join(map(str, r["shape"]))
            layout_str = f"{r['name']}|{r['dtype']}|{shape_str}|{r['offset']}|{r['nbytes']}|{r['alignment']}"
            actual_layout_sha256 = hashlib.sha256(layout_str.encode()).digest()
            if actual_layout_sha256 != r["layout_sha256"]:
                print(f"ZCC_TENSOR_ERR_LAYOUT_NONCANONICAL: Layout metadata hash mismatch for '{name}'")
                sys.exit(1)

            print(f"  - Tensor '{name}' verified: shape={r['shape']} type={get_dtype_name(r['dtype'])} payload=OK")

    # 6. Verify global manifest hash
    json_records = []
    for r in records:
        json_records.append({
            "name": r["name"],
            "dtype": r["dtype"],
            "rank": r["rank"],
            "shape": r["shape"],
            "offset": r["offset"],
            "nbytes": r["nbytes"],
            "alignment": r["alignment"],
            "flags": r["flags"],
            "tensor_sha256": r["tensor_sha256"].hex(),
            "layout_sha256": r["layout_sha256"].hex()
        })
    canonical_json = json.dumps(json_records, sort_keys=True, separators=(',', ':')).encode()
    computed_manifest_sha256 = hashlib.sha256(canonical_json).digest()

    if computed_manifest_sha256 != manifest_sha256:
        print("ZCC_TENSOR_ERR_LAYOUT_NONCANONICAL: Global manifest hash mismatch!")
        sys.exit(1)

    print("\n✅ Verification SUCCESS! Linked ELF executable matches GGUF tensor pipeline payload.")
    sys.exit(0)

if __name__ == '__main__':
    main()
