#!/usr/bin/env python3
import json
import hashlib
import numpy as np
from fastapi.testclient import TestClient
from cryptography.hazmat.primitives.asymmetric import ed25519
import app.fastapi_retrieval_gateway as gateway

client = TestClient(gateway.app)

def test_attestation_correctness():
    print("\n🔱 Starting Gate: Cryptographic Search Attestation Verification...")
    
    # 1. Query with a dummy 384-D vector
    rng = np.random.default_rng(1337)
    dummy_vector = rng.uniform(-1.0, 1.0, size=384).tolist()
    
    # Perform Search
    response = client.post("/search", json={"vector": dummy_vector, "k": 3})
    assert response.status_code == 200, f"Query failed with {response.status_code}"
    
    data = response.json()
    assert "attestation" in data, "Response missing attestation section!"
    
    attestation = data["attestation"]
    results = data["results"]
    
    # Extract keys and signatures
    epoch_hash = attestation["epoch_hash"]
    query_hash = attestation["query_hash"]
    results_hash = attestation["results_hash"]
    message_hex = attestation["message_hex"]
    signature_hex = attestation["signature_hex"]
    public_key_hex = attestation["public_key_hex"]
    
    print("🔱 Attestation Telemetry Harvested:")
    print(f"  -> Epoch Hash:   {epoch_hash}")
    print(f"  -> Query Hash:   {query_hash}")
    print(f"  -> Results Hash: {results_hash}")
    print(f"  -> Signature:    {signature_hex[:24]}...")
    
    # 2. Local verification to prove cryptographic correctness
    # A. Validate query hash matching
    query_bytes = np.asarray(dummy_vector, dtype="float32").tobytes()
    expected_query_hash = hashlib.sha256(query_bytes).hexdigest()
    assert query_hash == expected_query_hash, f"Query hash mismatch: {query_hash} vs {expected_query_hash}"
    
    # B. Validate results hash matching
    stable_results = [r["record"] for r in results]
    results_bytes = json.dumps(stable_results, sort_keys=True, separators=(',', ':')).encode('utf-8')
    expected_results_hash = hashlib.sha256(results_bytes).hexdigest()
    assert results_hash == expected_results_hash, f"Results hash mismatch: {results_hash} vs {expected_results_hash}"
    
    # C. Reconstruct message bytes
    expected_msg_bytes = bytes.fromhex(query_hash) + bytes.fromhex(results_hash) + bytes.fromhex(epoch_hash)
    assert message_hex == expected_msg_bytes.hex(), "Attestation message bytes mismatch!"
    
    # D. Perform cryptographic Ed25519 signature verification using sovereign root public key
    try:
        pub_key_bytes = bytes.fromhex(public_key_hex)
        pub_key = ed25519.Ed25519PublicKey.from_public_bytes(pub_key_bytes)
        
        sig_bytes = bytes.fromhex(signature_hex)
        pub_key.verify(sig_bytes, expected_msg_bytes)
        print("🔱 CRYPTOGRAPHIC ATTESTATION VERIFIED: Ed25519 Signature valid.")
    except Exception as e:
        raise AssertionError(f"🔱 Cryptographic Attestation Failed: Signature is invalid! Error: {e}")

def test_tombstone_filtering():
    print("\n🔱 Starting Gate: Tombstone Filter Verification...")
    
    # 1. Do a standard query to find the top match
    rng = np.random.default_rng(42)
    dummy_vector = rng.uniform(-1.0, 1.0, size=384).tolist()
    
    response = client.post("/search", json={"vector": dummy_vector, "k": 3})
    assert response.status_code == 200
    original_results = response.json()["results"]
    assert len(original_results) == 3
    
    top_contract = original_results[0]["record"]["contract"]
    second_contract = original_results[1]["record"]["contract"]
    print(f"  -> Original Top Match: {top_contract}")
    print(f"  -> Original Second Match: {second_contract}")
    
    # 2. Inject top match into the tombstone set dynamically
    gateway.tombstone_set.add(top_contract)
    print(f"  -> Tombstoned: {top_contract}")
    
    try:
        # Perform query again
        response = client.post("/search", json={"vector": dummy_vector, "k": 3})
        assert response.status_code == 200
        filtered_results = response.json()["results"]
        
        # Verify the top match has been dropped and second is promoted
        assert len(filtered_results) == 3, f"Expected 3 results, got {len(filtered_results)}"
        new_top_contract = filtered_results[0]["record"]["contract"]
        print(f"  -> New Top Match (Post-Filter): {new_top_contract}")
        
        assert new_top_contract == second_contract, f"Expected second contract {second_contract} to be promoted, got {new_top_contract}"
        
        # Make sure tombstoned contract is NOT in any of the returned results
        for r in filtered_results:
            assert r["record"]["contract"] != top_contract, f"Tombstoned contract {top_contract} was found in the filtered results!"
            
        print("🔱 TOMBSTONE FILTERING VERIFIED: Tombstoned records filtered and results properly padded.")
    finally:
        # Clean up
        gateway.tombstone_set.remove(top_contract)

if __name__ == "__main__":
    test_attestation_correctness()
    test_tombstone_filtering()
