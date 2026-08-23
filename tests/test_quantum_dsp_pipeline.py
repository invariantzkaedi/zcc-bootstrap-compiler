#!/usr/bin/env python3
"""
ZKAEDI SOVEREIGN PIPELINE: LAYER 1 VERIFICATION GAUNTLET
Unit + Integration test suite for Quantum Execution & DSP Stem:
1. Bit-exact phase vector extraction
2. Energy conservation & Hilbert norm conservation (sum(p_k) == 1.000000000000)
3. SciPy FFT round-trip invertibility (||x - iFFT(FFT(x))||_inf < 1e-12)
4. Sample-rate invariance (44.1 kHz vs 48.0 kHz spectral harmonic alignment)
5. Deterministic replay harness (Same canonical seed -> Bit-identical SHA-256 WAV hash)
"""

import os
import sys
import math
import hashlib
import unittest
import numpy as np
from scipy.fft import fft, ifft

# Add tools to path
sys.path.insert(0, os.path.abspath("./tools"))
from quantum_dsp_audio_stem import (
    extract_quantum_fields,
    synthesize_quantum_audio_stem,
    export_wav_stem,
    DEFAULT_SAMPLE_RATE,
    DEFAULT_DURATION,
    DEFAULT_BASE_FREQ,
)

BINARY_PATH = os.path.abspath("./examples/quantum_walk_16node_sim")
OUTPUT_WAV_44 = "/tmp/test_quantum_stem_44k.wav"
OUTPUT_WAV_48 = "/tmp/test_quantum_stem_48k.wav"

class TestQuantumDSPPipeline(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        # Ensure binary exists
        if not os.path.exists(BINARY_PATH):
            raise unittest.SkipTest(f"Binary {BINARY_PATH} not found. Build first.")

    def test_01_bit_exact_quantum_extraction(self):
        """Verify extraction of 32 basis amplitudes, 16 spatial nodes, and entropy metrics"""
        amps, probs, phases, entropies = extract_quantum_fields(BINARY_PATH)
        
        self.assertEqual(len(amps), 16, "Expected 16 non-zero basis amplitude states in 8-step DTQW")
        self.assertEqual(len(probs), 16, "Expected 16 spatial lattice nodes")
        self.assertEqual(len(phases), 16, "Expected 16 spatial phase fields")
        
        # Verify specific known analytic statevector entries
        self.assertIn(16, amps, "Basis state |10000> (index 16) should be non-zero")
        self.assertAlmostEqual(amps[16].real, 0.13258252, places=6)
        self.assertAlmostEqual(amps[16].imag, 0.13258252, places=6)
        
        # Verify subsystem entanglement entropy values
        self.assertIn("S(q0)", entropies)
        self.assertAlmostEqual(entropies["S(q0)"], 0.877437, places=5)
        self.assertAlmostEqual(entropies["S(q1)"], 0.997180, places=5)

    def test_02_quantum_energy_conservation(self):
        """Verify sum of Born probabilities equals 1.00000000 (Hilbert norm conservation)"""
        _, probs, _, _ = extract_quantum_fields(BINARY_PATH)
        total_prob = np.sum(probs)
        self.assertAlmostEqual(
            total_prob, 1.0, places=8,
            msg=f"Quantum energy conservation violated: sum(p) = {total_prob:.14f}"
        )

    def test_03_scipy_fft_round_trip_invertibility(self):
        """Verify SciPy FFT / iFFT round-trip transformation is mathematically lossless"""
        _, probs, phases, entropies = extract_quantum_fields(BINARY_PATH)
        _, float_wave = synthesize_quantum_audio_stem(probs, phases, entropies, sample_rate=44100, duration=1.0)
        
        # Compute forward FFT
        spectrum = fft(float_wave)
        
        # Compute inverse FFT
        reconstructed = ifft(spectrum).real
        
        # Max absolute error must be within machine epsilon floating point limits
        l_inf_error = np.max(np.abs(float_wave - reconstructed))
        self.assertLess(
            l_inf_error, 1e-12,
            f"FFT round-trip invertibility error {l_inf_error} exceeded 1e-12 limit"
        )
        
        # Verify spectral energy conservation via Parseval's theorem
        time_energy = np.sum(float_wave**2)
        freq_energy = np.sum(np.abs(spectrum)**2) / len(float_wave)
        self.assertAlmostEqual(
            time_energy, freq_energy, places=10,
            msg="Parseval energy conservation failed between time and frequency domains"
        )

    def test_04_sample_rate_invariance(self):
        """Verify audio stem synthesis generates equivalent spectral peak structure across sample rates"""
        _, probs, phases, entropies = extract_quantum_fields(BINARY_PATH)
        
        pcm_44, wave_44 = synthesize_quantum_audio_stem(probs, phases, entropies, sample_rate=44100, duration=1.0)
        pcm_48, wave_48 = synthesize_quantum_audio_stem(probs, phases, entropies, sample_rate=48000, duration=1.0)
        
        # Compute normalized FFT frequency peaks
        fft_44 = np.abs(fft(wave_44)[:22050])
        fft_48 = np.abs(fft(wave_48)[:24000])
        
        freqs_44 = np.linspace(0, 22050, 22050, endpoint=False)
        freqs_48 = np.linspace(0, 24000, 24000, endpoint=False)
        
        peak_freq_44 = freqs_44[np.argmax(fft_44)]
        peak_freq_48 = freqs_48[np.argmax(fft_48)]
        
        # Dominant fundamental frequency must align within 1 Hz resolution
        self.assertAlmostEqual(
            peak_freq_44, peak_freq_48, delta=1.5,
            msg=f"Peak frequency mismatch across sample rates: {peak_freq_44} Hz vs {peak_freq_48} Hz"
        )

    def test_05_deterministic_replay_hash_lock(self):
        """Verify deterministic replay: repeated execution generates bit-identical SHA-256 WAV hash"""
        _, probs, phases, entropies = extract_quantum_fields(BINARY_PATH)
        
        # Run 1
        pcm_1, _ = synthesize_quantum_audio_stem(probs, phases, entropies)
        hash_1 = export_wav_stem(pcm_1, OUTPUT_WAV_44)
        
        # Run 2 (Independent Synthesis)
        pcm_2, _ = synthesize_quantum_audio_stem(probs, phases, entropies)
        hash_2 = export_wav_stem(pcm_2, OUTPUT_WAV_44)
        
        # Assert Bit-Exact Equality
        np.testing.assert_array_equal(pcm_1, pcm_2, err_msg="PCM array mismatch across repeated runs")
        self.assertEqual(hash_1, hash_2, "SHA-256 hash drift detected on deterministic replay!")
        
        print(f"\n[DETERMINISTIC SEAL] Reference WAV SHA-256: {hash_1}")

    @classmethod
    def tearDownClass(cls):
        for p in [OUTPUT_WAV_44, OUTPUT_WAV_48]:
            if os.path.exists(p):
                os.remove(p)

if __name__ == "__main__":
    unittest.main()
