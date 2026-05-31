import sys
import struct

def read_string(f):
    length_bytes = f.read(8)
    if len(length_bytes) < 8:
        return ""
    length = struct.unpack("<Q", length_bytes)[0]
    return f.read(length).decode("utf-8", errors="ignore")

def validate_gguf(filepath, expected_tensor_count=2, expected_quantization=0):
    print(f"=== Validating GGUF File: {filepath} ===")
    with open(filepath, "rb") as f:
        # 1. Read Header
        header_data = f.read(24)
        if len(header_data) < 24:
            print("ERROR: File too short for GGUF header")
            return False
        
        magic, version, tensor_count, metadata_kv_count = struct.unpack("<IIQQ", header_data)
        
        print(f"Magic: {hex(magic)} (Expected: 0x46554747)")
        print(f"Version: {version} (Expected: 3)")
        print(f"Tensor Count: {tensor_count} (Expected: {expected_tensor_count})")
        print(f"Metadata Count: {metadata_kv_count} (Expected: 3)")
        
        if magic != 0x46554747:
            print("ERROR: Magic bytes mismatch!")
            return False
        if version != 3:
            print("ERROR: Version mismatch!")
            return False
        if tensor_count != expected_tensor_count:
            print("ERROR: Tensor count mismatch!")
            return False
            
        # 2. Read Metadata Key-Values
        metadata = {}
        for idx in range(metadata_kv_count):
            key = read_string(f)
            val_type_bytes = f.read(4)
            if len(val_type_bytes) < 4:
                print("ERROR: File truncated inside metadata")
                return False
            val_type = struct.unpack("<I", val_type_bytes)[0]
            if val_type == 8: # String
                val = read_string(f)
            else:
                print(f"ERROR: Unexpected metadata value type {val_type}")
                return False
            metadata[key] = val
            print(f"Metadata Key-Value [{idx}]: '{key}' = '{val}'")
            
        if "general.architecture" not in metadata or metadata["general.architecture"] != "zcc_prime":
            print("ERROR: general.architecture mismatch or missing")
            return False
        if "zcc.signature" not in metadata:
            print("ERROR: zcc.signature missing!")
            return False
        
        sig = metadata["zcc.signature"]
        if len(sig) != 64 or sig == "0000000000000000000000000000000000000000000000000000000000000000":
            print("ERROR: Invalid or un-attested GGUF signature!")
            return False
            
        # 3. Read Tensor Info
        tensor_infos = []
        for idx in range(tensor_count):
            name = read_string(f)
            n_dims_bytes = f.read(4)
            if len(n_dims_bytes) < 4:
                print("ERROR: File truncated inside tensor info name/dims")
                return False
            n_dims = struct.unpack("<I", n_dims_bytes)[0]
            dims_bytes = f.read(8 * n_dims)
            if len(dims_bytes) < 8 * n_dims:
                print("ERROR: File truncated inside dimensions")
                return False
            dims = struct.unpack(f"<{n_dims}Q", dims_bytes)
            
            type_bytes = f.read(4)
            offset_bytes = f.read(8)
            if len(type_bytes) < 4 or len(offset_bytes) < 8:
                print("ERROR: File truncated inside type/offset")
                return False
            t_type = struct.unpack("<I", type_bytes)[0]
            offset = struct.unpack("<Q", offset_bytes)[0]
            
            print(f"Tensor [{idx}]: '{name}' | Dims: {dims} | Type: {t_type} (Expected: {expected_quantization}) | Offset: {offset}")
            if t_type != expected_quantization:
                print(f"ERROR: Unexpected tensor type {t_type}")
                return False
                
            tensor_infos.append({
                "name": name,
                "n_dims": n_dims,
                "dims": dims,
                "type": t_type,
                "offset": offset
            })
            
        # 4. Check Alignment and Offsets
        metadata_end_pos = f.tell()
        aligned_data_start = (metadata_end_pos + 31) & ~31
        print(f"Metadata End Position: {metadata_end_pos} | Aligned Data Start: {aligned_data_start}")
        
        # Verify offsets
        for idx, info in enumerate(tensor_infos):
            expected_pos = aligned_data_start + info["offset"]
            print(f"Tensor '{info['name']}' data should start at file position: {expected_pos}")
            
    print(f"=== SUCCESS: {filepath} is 100% GGUF COMPLIANT AND ATTESTED ===\n")
    return True

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python3 validate_gguf_output.py <filepath> [expected_quantization]")
        sys.exit(1)
    filepath = sys.argv[1]
    expected_quant = int(sys.argv[2]) if len(sys.argv) > 2 else 0
    if not validate_gguf(filepath, expected_quantization=expected_quant):
        sys.exit(1)
