# Ticket: PLATFORM_TOOL_RECONSTRUCTION_DISCREPANCY
**Status:** Open
**Type:** Platform Tooling / Verification
**Reporter:** Antigravity (AI assistant)
**Date:** 2026-07-07

## Symptoms
The platform's file-viewing tools (`view_file`, etc.) can occasionally introduce transcription or case discrepancies (such as lowercasing Python boolean `False` to `false`) due to reconstruction/formatting steps in the tool-response rendering layer. This creates a semantic risk where the code presented for user review does not match the actual bytes saved on disk.

## Workaround
To preserve exact byte-level correctness when presenting code or execution traces for review, the assistant should:
1. Prefer raw terminal print commands (e.g. `cat` or `type` via the terminal sandbox) over high-level viewing tools.
2. Provide explicit SHA-256 or MD5 checksums of files alongside code snippets when high-level tools are used, enabling the user to run independent integrity checks.

## Root Cause & Proposed Resolution
The platform execution layers format, serialize, or summarize tool results before injecting them into the chat context. Fixing this requires implementing a strict, byte-preserving pass-through option for files containing code extensions (.py, .ts, .c, .rs) that guarantees zero semantic transformation.
