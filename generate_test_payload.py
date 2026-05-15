import struct
import os

filepath = "/tmp/mock_glb_payload.bin"

# We simulate a 5-vertex Custom Binary Attribute payload exactly as it
# exists inside the _rigged.glb binary buffer chunk.

# 1. Triangle Indices (UInt32)
indices = [42, 109, 7, 999, 13]

# 2. Barycentric UVW Offsets (Float32)
uvws = [
    0.5, 0.25, 0.25,    # Inside Triangle
    1.2, -0.1, -0.1,    # Outside (Negative Extrapolation)
    0.33, 0.33, 0.34,   # Dead Center
    0.0, 1.0, 0.0,      # Exact Vertex Match
    0.9, 0.15, -0.05    # Border Edge Case
]

# Write out the raw memory struct
with open(filepath, "wb") as f:
    for idx in indices:
        f.write(struct.pack("<I", idx))
    for uvw in uvws:
        f.write(struct.pack("<f", uvw))

byte_offset = 0
byte_length = (len(indices) * 4) + (len(uvws) * 4)

print(f"✅ Mock payload written to {filepath}")
print("Run the following to test C-Native visual ingestion:")
print(f"wsl /tmp/zcc_slave_test {filepath} {byte_offset} {byte_length} 5")
