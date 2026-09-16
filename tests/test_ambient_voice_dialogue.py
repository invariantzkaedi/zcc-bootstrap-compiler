# -*- coding: utf-8 -*-
"""
Unit tests for Autonomous Audio Dialogue Loop.
"""

import sys
import unittest
import numpy as np
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from tools.npu.ambient_voice_dialogue import AutonomousAudioDialogueLoop, AudioRingBuffer


class TestAutonomousAudioDialogueLoop(unittest.TestCase):

    def setUp(self):
        self.dialogue = AutonomousAudioDialogueLoop(device_id=1)

    def test_audio_ring_buffer_circular_wrap(self):
        buf = AudioRingBuffer(capacity_seconds=1.0, sample_rate=16000)
        data = np.ones(10000, dtype=np.float32)
        buf.write(data)
        self.assertEqual(buf.total_samples_written, 10000)

        # Write another chunk that wraps around 16,000 capacity
        data2 = np.ones(8000, dtype=np.float32) * 2.0
        buf.write(data2)
        self.assertEqual(buf.total_samples_written, 18000)

        # Retrieve window of 0.25s (4000 samples)
        win = buf.get_latest_window(duration_s=0.25)
        self.assertEqual(len(win), 4000)
        self.assertTrue(np.all(win == 2.0))

    def test_audio_ingestion_and_vad(self):
        # Silence chunk
        silence = np.zeros(8000, dtype=np.float32)
        res_silence = self.dialogue.ingest_audio(silence)
        self.assertFalse(res_silence["vad_active"])

        # High-energy tone chunk
        tone = np.sin(2.0 * np.pi * 440.0 * np.linspace(0, 0.5, 8000)).astype(np.float32) * 0.5
        res_tone = self.dialogue.ingest_audio(tone)
        self.assertTrue(res_tone["vad_active"])

    def test_spoken_command_and_chime_generation(self):
        cmd = "Zkaedi, test bitwise union casting with ZCC"
        turn_res = self.dialogue.process_spoken_command(cmd)

        self.assertEqual(turn_res["action_taken"], "EXECUTE_SOVEREIGN_SYMPHONY_JIT")
        self.assertEqual(turn_res["jit_execution"]["verdict"], "PASS")
        self.assertGreater(turn_res["chime_bytes_len"], 1000)
        self.assertLess(turn_res["total_turn_latency_ms"], 25.0)


if __name__ == "__main__":
    unittest.main()
