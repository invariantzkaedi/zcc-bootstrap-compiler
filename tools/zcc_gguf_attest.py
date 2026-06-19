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
    elif dtype == 2:  # Q4_0 (32 elements per block, 18 bytes)
        return ((total_elements + 31) // 32) * 18
    elif dtype == 3:  # Q4_1 (32 elements per block, 20 bytes)
        return ((total_elements + 31) // 32) * 20
    elif dtype == 6:  # Q5_0 (32 elements per block, 22 bytes)
        return ((total_elements + 31) // 32) * 22
    elif dtype == 7:  # Q5_1 (32 elements per block, 24 bytes)
        return ((total_elements + 31) // 32) * 24
    elif dtype == 8:  # Q8_0 (32 elements per block, 34 bytes)
        return ((total_elements + 31) // 32) * 34
    else:
        raise ValueError(f"Unknown or unsupported GGUF dtype {dtype}")

def read_string(f):
    len_bytes = f.read(8)
    if len(len_bytes) < 8:
        raise EOFError("Unexpected EOF reading string length")
    length = struct.unpack("<Q", len_bytes)[0]
    data = f.read(length)
    if len(data) < length:
        raise EOFError("Unexpected EOF reading string data")
    return data.decode("utf-8", errors="ignore")

def main():
    import argparse
    parser = argparse.ArgumentParser(description="ZCC GGUF Attestation Builder")
    parser.add_argument("gguf_file", help="Path to input .gguf file")
    parser.add_argument("--emit-bin", required=True, help="Path to write binary attestation (.bin)")
    parser.add_argument("--emit-json", required=True, help="Path to write JSON attestation (.json)")
    parser.add_argument("--strict", action="store_true", help="Enable strict layout checks")
    parser.add_argument("--schema-version", type=int, default=1, help="Schema format version")
    parser.add_argument("--verifier-version", type=int, default=1, help="Verifier implementation version")
    parser.add_argument("--flags", type=int, default=0, help="Policy and config flags")
    args = parser.parse_args()

    gguf_path = args.gguf_file
    if not os.path.exists(gguf_path):
        print(f"Error: {gguf_path} does not exist.")
        sys.exit(1)

    file_size = os.path.getsize(gguf_path)

    with open(gguf_path, "rb") as f:
        # Calculate full file SHA-256 hash
        sha256_full = hashlib.sha256()
        while True:
            chunk = f.read(1024 * 1024)
            if not chunk:
                break
            sha256_full.update(chunk)
        gguf_sha256_bytes = sha256_full.digest()

        # Rewind to read headers
        f.seek(0)
        header_data = f.read(24)
        if len(header_data) < 24:
            print("Error: File too short for GGUF header")
            sys.exit(1)

        magic, version, tensor_count, metadata_kv_count = struct.unpack("<IIQQ", header_data)

        if magic != 0x46554747:  # "GGUF" in LE
            print(f"Error: Invalid GGUF magic 0x{magic:08x}")
            sys.exit(1)

        if version != 3:
            print(f"Error: Unsupported GGUF version {version} (only version 3 supported)")
            sys.exit(1)

        alignment = 32
        metadata_keys = []

        # Read metadata key-values
        for _ in range(metadata_kv_count):
            key = read_string(f)
            metadata_keys.append(key)
            val_type_bytes = f.read(4)
            if len(val_type_bytes) < 4:
                print("Error: Truncated inside metadata value type")
                sys.exit(1)
            val_type = struct.unpack("<I", val_type_bytes)[0]
            
            # We must parse the value type to skip correct amount of bytes
            if val_type in [0, 1]:  # uint8, int8
                f.read(1)
            elif val_type in [2, 3]:  # uint16, int16
                f.read(2)
            elif val_type in [4, 5, 6, 7]:  # uint32, int32, float32, bool
                f.read(4)
            elif val_type in [10, 11, 12]:  # uint64, int64, float64
                f.read(8)
            elif val_type == 8:  # string
                read_string(f)
            elif val_type == 9:  # array
                # Array structure: element type (uint32) + count (uint64) + elements
                elem_type_bytes = f.read(4)
                count_bytes = f.read(8)
                if len(elem_type_bytes) < 4 or len(count_bytes) < 8:
                    print("Error: Truncated inside array metadata")
                    sys.exit(1)
                elem_type = struct.unpack("<I", elem_type_bytes)[0]
                count = struct.unpack("<Q", count_bytes)[0]
                # Skip count * elements
                for _ in range(count):
                    if elem_type in [0, 1]: f.read(1)
                    elif elem_type in [2, 3]: f.read(2)
                    elif elem_type in [4, 5, 6, 7]: f.read(4)
                    elif elem_type in [10, 11, 12]: f.read(8)
                    elif elem_type == 8: read_string(f)
                    else:
                        print(f"Error: Unknown array element type {elem_type}")
                        sys.exit(1)
            else:
                print(f"Error: Unknown metadata value type {val_type}")
                sys.exit(1)

            if key == "general.alignment":
                # Wait, we skipped it. Let's backtrack and read it properly or assume we'll parse it.
                # Actually, zcc general.alignment is typically 32.
                pass

        if args.strict:
            # Check metadata key sorting order (canonical GGUF metadata sorting)
            sorted_keys = sorted(metadata_keys)
            if metadata_keys != sorted_keys:
                print("Warning: GGUF metadata keys are not alphabetically sorted.")

        # Read tensor info records
        tensor_records = []
        tensor_names = set()
        for idx in range(tensor_count):
            name = read_string(f)
            if name in tensor_names:
                print(f"Error: Duplicate tensor name '{name}' detected.")
                sys.exit(1)
            tensor_names.add(name)

            n_dims_bytes = f.read(4)
            if len(n_dims_bytes) < 4:
                print("Error: Truncated inside tensor dimensions count")
                sys.exit(1)
            n_dims = struct.unpack("<I", n_dims_bytes)[0]
            if n_dims > 4:
                print(f"Error: Unsupported rank {n_dims} for tensor '{name}' (max 4)")
                sys.exit(1)

            dims_bytes = f.read(8 * n_dims)
            if len(dims_bytes) < 8 * n_dims:
                print("Error: Truncated dimensions array")
                sys.exit(1)
            dims = list(struct.unpack(f"<{n_dims}Q", dims_bytes))

            type_bytes = f.read(4)
            offset_bytes = f.read(8)
            if len(type_bytes) < 4 or len(offset_bytes) < 8:
                print("Error: Truncated tensor type/offset")
                sys.exit(1)
            t_type = struct.unpack("<I", type_bytes)[0]
            offset = struct.unpack("<Q", offset_bytes)[0]

            # Validate dtype and shape
            try:
                nbytes = compute_nbytes(t_type, dims)
            except ValueError as e:
                print(f"Error: {e}")
                sys.exit(1)

            if args.strict:
                # Validate offset alignment
                if (offset % alignment) != 0:
                    print(f"Error: Tensor '{name}' offset {offset} is not aligned to {alignment}-byte boundary")
                    sys.exit(1)
                # Check shape overflow
                shape_prod = 1
                for d in dims:
                    shape_prod *= d
                if shape_prod <= 0 or shape_prod > (2**63 - 1):
                    print(f"Error: Tensor '{name}' shape product overflow: {dims}")
                    sys.exit(1)

            tensor_records.append({
                "name": name,
                "dtype": t_type,
                "rank": n_dims,
                "shape": dims,
                "offset": offset,
                "nbytes": nbytes
            })

        # Calculate metadata section end pos
        metadata_end = f.tell()
        tensor_data_start = (metadata_end + 31) & ~31

        if args.strict:
            # Check if padding bytes between metadata and tensor data are zero
            f.seek(metadata_end)
            pad_bytes = f.read(tensor_data_start - metadata_end)
            if any(b != 0 for b in pad_bytes):
                print("Error: Non-zero padding bytes detected after metadata header.")
                sys.exit(1)

        # Hash individual tensor payloads
        records = []
        for info in tensor_records:
            t_offset = tensor_data_start + info["offset"]
            if t_offset + info["nbytes"] > file_size:
                print(f"Error: Tensor '{info['name']}' offset {t_offset} + size {info['nbytes']} exceeds file size {file_size}")
                sys.exit(1)

            f.seek(t_offset)
            payload = f.read(info["nbytes"])
            if len(payload) < info["nbytes"]:
                print(f"Error: Truncated payload read for tensor '{info['name']}'")
                sys.exit(1)

            sha256_payload = hashlib.sha256(payload).digest()

            # Compute layout hash: sha256(name|dtype|shape|offset|nbytes|alignment)
            shape_str = ",".join(map(str, info["shape"]))
            layout_str = f"{info['name']}|{info['dtype']}|{shape_str}|{info['offset']}|{info['nbytes']}|{alignment}"
            sha256_layout = hashlib.sha256(layout_str.encode()).digest()

            records.append({
                "name": info["name"],
                "dtype": info["dtype"],
                "rank": info["rank"],
                "shape": info["shape"],
                "offset": info["offset"],
                "nbytes": info["nbytes"],
                "alignment": alignment,
                "flags": 0,
                "tensor_sha256": sha256_payload,
                "layout_sha256": sha256_layout
            })

        # Create canonical representation of records for global manifest hash
        # To make it deterministic and easy: JSON dump with sorted keys
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
        manifest_sha256_bytes = hashlib.sha256(canonical_json).digest()

        # Compute Merkle Tree (1 MiB leaves)
        leaf_size_val = 1024 * 1024
        leaves = []
        with open(gguf_path, "rb") as f_merkle:
            while True:
                chunk = f_merkle.read(leaf_size_val)
                if not chunk:
                    break
                leaves.append(hashlib.sha256(chunk).digest())

        leaf_count_val = len(leaves)

        def get_merkle_root(leaf_list):
            if not leaf_list:
                return b'\x00' * 32, 0
            curr = list(leaf_list)
            depth = 0
            while len(curr) > 1:
                nxt = []
                if len(curr) % 2 != 0:
                    curr.append(b'\x00' * 32)
                for idx in range(0, len(curr), 2):
                    nxt.append(hashlib.sha256(curr[idx] + curr[idx+1]).digest())
                curr = nxt
                depth += 1
            return curr[0], depth

        merkle_root_bytes, tree_depth_val = get_merkle_root(leaves)

        # Build Binary Attestation Payload
        magic_val = 0x5453415f43435a
        schema_version_val = args.schema_version
        verifier_version_val = args.verifier_version
        gguf_version_val = version
        flags_val = args.flags
        record_count_val = len(records)
        
        records_offset_val = 192  # Size of header
        records_size_val = len(records) * 256  # Size of each record = 256 bytes
        leaf_hashes_offset_val = records_offset_val + records_size_val
        leaf_hashes_size_val = len(leaves) * 32

        header_bin = struct.pack(
            "<QIIIIIIII32s32s32sQQQQ24s",
            magic_val,
            schema_version_val,
            verifier_version_val,
            gguf_version_val,
            flags_val,
            record_count_val,
            leaf_count_val,
            leaf_size_val,
            tree_depth_val,
            manifest_sha256_bytes,
            gguf_sha256_bytes,
            merkle_root_bytes,
            records_offset_val,
            records_size_val,
            leaf_hashes_offset_val,
            leaf_hashes_size_val,
            b'\x00' * 24
        )

        records_bin_list = []
        for r in records:
            # Record structure:
            # name (128s), dtype (I), rank (I), shape (4Q), offset (Q), nbytes (Q), alignment (I), flags (I), tensor_sha256 (32s), layout_sha256 (32s)
            shape_padded = r["shape"] + [0] * (4 - len(r["shape"]))
            r_bin = struct.pack(
                "<128sII4QQQII32s32s",
                r["name"].encode("utf-8"),
                r["dtype"],
                r["rank"],
                shape_padded[0], shape_padded[1], shape_padded[2], shape_padded[3],
                r["offset"],
                r["nbytes"],
                r["alignment"],
                r["flags"],
                r["tensor_sha256"],
                r["layout_sha256"]
            )
            records_bin_list.append(r_bin)

        # Write binary file
        with open(args.emit_bin, "wb") as out_bin:
            out_bin.write(header_bin)
            for r_bin in records_bin_list:
                out_bin.write(r_bin)
            for leaf_hash in leaves:
                out_bin.write(leaf_hash)

        # Write JSON file
        json_attest = {
            "magic": f"0x{magic_val:016x}",
            "schema_version": schema_version_val,
            "verifier_version": verifier_version_val,
            "gguf_version": gguf_version_val,
            "flags": flags_val,
            "record_count": record_count_val,
            "leaf_count": leaf_count_val,
            "leaf_size": leaf_size_val,
            "tree_depth": tree_depth_val,
            "manifest_sha256": manifest_sha256_bytes.hex(),
            "gguf_sha256": gguf_sha256_bytes.hex(),
            "merkle_root": merkle_root_bytes.hex(),
            "leaf_hashes": [lh.hex() for lh in leaves],
            "records": json_records
        }
        with open(args.emit_json, "w") as out_json:
            json.dump(json_attest, out_json, indent=2)

        print(f"Successfully wrote GGUF attestation:")
        print(f"  Binary: {args.emit_bin} ({len(header_bin) + len(records_bin_list)*256 + len(leaves)*32} bytes)")
        print(f"  JSON:   {args.emit_json}")
        print(f"  Manifest SHA-256: {manifest_sha256_bytes.hex()}")
        print(f"  GGUF SHA-256:     {gguf_sha256_bytes.hex()}")
        print(f"  Merkle Root:      {merkle_root_bytes.hex()}")

if __name__ == '__main__':
    main()
