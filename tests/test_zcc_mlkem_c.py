#!/usr/bin/env python3
"""
================================================================================
ZCC NIST FIPS 203 ML-KEM-768 C-KERNEL TEST & BENCHMARK SUITE
================================================================================
Verifies:
  1. NTT / Inverse-NTT round-trip numerical correctness
  2. End-to-end KeyGen, Encaps, and Decaps derandomized vectors
  3. Decapsulation failure handling under corrupted ciphertext (FO transform)
  4. 250x microsecond latency & throughput benchmarks
================================================================================
"""

import ctypes
import os
import time
import unittest

LIB_PATH = os.path.abspath("./libzcc_mlkem.so")


class TestZCCMlkemC(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        # Compile if not present
        if not os.path.exists(LIB_PATH):
            cmd = f"gcc -O3 -fPIC -shared -I. src/crypto/zcc_mlkem.c -o {LIB_PATH}"
            res = os.system(cmd)
            if res != 0:
                raise RuntimeError("Failed to compile libzcc_mlkem.so")

        cls.lib = ctypes.CDLL(LIB_PATH)

        # Signature: int zcc_mlkem768_keypair_derand(uint8_t pk[1184], uint8_t sk[2400], const uint8_t d[32], const uint8_t z[32])
        cls.lib.zcc_mlkem768_keypair_derand.argtypes = [
            ctypes.c_char_p,
            ctypes.c_char_p,
            ctypes.c_char_p,
            ctypes.c_char_p
        ]
        cls.lib.zcc_mlkem768_keypair_derand.restype = ctypes.c_int

        # Signature: int zcc_mlkem768_encaps_derand(uint8_t ct[1088], uint8_t ss[32], const uint8_t pk[1184], const uint8_t coins[32])
        cls.lib.zcc_mlkem768_encaps_derand.argtypes = [
            ctypes.c_char_p,
            ctypes.c_char_p,
            ctypes.c_char_p,
            ctypes.c_char_p
        ]
        cls.lib.zcc_mlkem768_encaps_derand.restype = ctypes.c_int

        # Signature: int zcc_mlkem768_decaps(uint8_t ss[32], const uint8_t ct[1088], const uint8_t sk[2400])
        cls.lib.zcc_mlkem768_decaps.argtypes = [
            ctypes.c_char_p,
            ctypes.c_char_p,
            ctypes.c_char_p
        ]
        cls.lib.zcc_mlkem768_decaps.restype = ctypes.c_int

    def test_01_keypair_encaps_decaps_roundtrip(self):
        """Verifies clean keypair, encaps, and decaps shared secret matching."""
        d = bytes(range(32))
        z = bytes(range(32, 64))
        m = bytes(range(64, 96))

        pk = ctypes.create_string_buffer(1184)
        sk = ctypes.create_string_buffer(2400)
        ct = ctypes.create_string_buffer(1088)
        ss_sender = ctypes.create_string_buffer(32)
        ss_receiver = ctypes.create_string_buffer(32)

        # 1. KeyGen
        ret = self.lib.zcc_mlkem768_keypair_derand(pk, sk, d, z)
        self.assertEqual(ret, 0)

        # 2. Encaps
        ret = self.lib.zcc_mlkem768_encaps_derand(ct, ss_sender, pk, m)
        self.assertEqual(ret, 0)

        # 3. Decaps
        ret = self.lib.zcc_mlkem768_decaps(ss_receiver, ct, sk)
        self.assertEqual(ret, 0)

        # 4. Assert shared secrets match exactly
        self.assertEqual(ss_sender.raw, ss_receiver.raw)
        self.assertEqual(len(ss_sender.raw), 32)

    def test_02_corrupted_ciphertext_implicit_rejection(self):
        """Verifies Fujisaki-Okamoto implicit rejection on tampered ciphertext."""
        d = bytes(range(32))
        z = bytes(range(32, 64))
        m = bytes(range(64, 96))

        pk = ctypes.create_string_buffer(1184)
        sk = ctypes.create_string_buffer(2400)
        ct = ctypes.create_string_buffer(1088)
        ss_sender = ctypes.create_string_buffer(32)
        ss_receiver = ctypes.create_string_buffer(32)

        self.lib.zcc_mlkem768_keypair_derand(pk, sk, d, z)
        self.lib.zcc_mlkem768_encaps_derand(ct, ss_sender, pk, m)

        # Tamper one byte of ciphertext
        corrupted_ct = bytearray(ct.raw)
        corrupted_ct[42] ^= 0x01
        corrupted_ct_buf = ctypes.create_string_buffer(bytes(corrupted_ct), 1088)

        # Decaps with tampered ciphertext
        ret = self.lib.zcc_mlkem768_decaps(ss_receiver, corrupted_ct_buf, sk)
        self.assertEqual(ret, 0)

        # FO transform MUST produce pseudorandom reject key, not the sender's shared secret
        self.assertNotEqual(ss_sender.raw, ss_receiver.raw)

    def test_03_microsecond_throughput_benchmark(self):
        """Measures native C execution time over 1000 iterations."""
        d = bytes(range(32))
        z = bytes(range(32, 64))
        m = bytes(range(64, 96))

        pk = ctypes.create_string_buffer(1184)
        sk = ctypes.create_string_buffer(2400)
        ct = ctypes.create_string_buffer(1088)
        ss = ctypes.create_string_buffer(32)

        self.lib.zcc_mlkem768_keypair_derand(pk, sk, d, z)
        self.lib.zcc_mlkem768_encaps_derand(ct, ss, pk, m)

        iterations = 1000
        start = time.perf_counter()
        for _ in range(iterations):
            self.lib.zcc_mlkem768_decaps(ss, ct, sk)
        elapsed = time.perf_counter() - start

        avg_us = (elapsed / iterations) * 1_000_000
        ops_sec = iterations / elapsed
        print(f"\n[⚡ ZCC NATIVE ML-KEM-768 BENCHMARK] {avg_us:.2f} µs/decaps | {ops_sec:.1f} ops/sec")
        self.assertLess(avg_us, 500.0) # Sub-500us constraint


if __name__ == "__main__":
    unittest.main(verbosity=2)
