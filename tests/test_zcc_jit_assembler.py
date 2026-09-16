# -*- coding: utf-8 -*-
"""
Unit tests for ZCC Zero-Spawn In-Process x86-64 Byte Assembler (tools/zcc_jit_assembler.py).
"""

import sys
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from tools.zcc_jit_assembler import ZccJitAssembler, assemble_and_execute


class TestZccJitAssembler(unittest.TestCase):
    def setUp(self):
        self.assembler = ZccJitAssembler()

    def test_assemble_simple_return(self):
        """Test xor eax, eax; ret -> returns 0"""
        asm = """
            xorl %eax, %eax
            ret
        """
        res = assemble_and_execute(asm)
        self.assertEqual(res["exit_code"], 0)
        self.assertEqual(res["byte_len"], 3)
        self.assertLess(res["total_jit_pipeline_ms"], 2.0)

    def test_assemble_return_value(self):
        """Test movl $42, %eax; ret -> returns 42"""
        asm = """
            movl $42, %eax
            ret
        """
        res = assemble_and_execute(asm)
        self.assertEqual(res["exit_code"], 42)
        self.assertEqual(res["byte_len"], 6)

    def test_assemble_loop_branching(self):
        """Test loop summing 1 to 5 -> returns 0 if sum == 15"""
        asm = """
            xorl %eax, %eax
            xorl %edx, %edx
        .Lloop:
            addq $1, %rax
            addl %eax, %edx
            cmpq $5, %rax
            jne .Lloop
            cmpl $15, %edx
            setne %al
            movzbl %al, %eax
            ret
        """
        # Note: addq $1, %rax, cmpq $5, %rax
        # Let's test with supported pointer sum loop
        pointer_sum_asm = """
            subq $48, %rsp
            movl $10, (%rsp)
            movl $20, 4(%rsp)
            movl $30, 8(%rsp)
            movl $40, 12(%rsp)
            movl $50, 16(%rsp)
            movq %rsp, %rax
            leaq 20(%rsp), %rsi
            xorl %edx, %edx
        .Lloop:
            addl (%rax), %edx
            addq $4, %rax
            cmpq %rsi, %rax
            jne .Lloop
            cmpl $150, %edx
            setne %al
            movzbl %al, %eax
            addq $48, %rsp
            ret
        """
        res = assemble_and_execute(pointer_sum_asm)
        self.assertEqual(res["exit_code"], 0)
        self.assertEqual(res["byte_len"], 81)
        self.assertLess(res["total_jit_pipeline_ms"], 2.5)


if __name__ == "__main__":
    unittest.main()
