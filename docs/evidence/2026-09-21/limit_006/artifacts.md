# Key Artifacts: LIMIT-006 (Dual-Register Struct Return in SSA-IR: `%rax/%rdx`)

| Artifact Path | SHA-256 Checksum | Description |
| :--- | :--- | :--- |
| `tests/probe_ret2.c` | `80bf66061e6999e8441e9562edecdd7d10374bac3235d7940692f83071254854` | Rule EF-3 minimal reproduction probe for dual-register struct return |
| `zcc2.s` | `0f8398bbed202e4175552f1f2bb35668d6653ff0917de30fb741ac401a8f8df6` | Stage 2 compiler assembly output |
| `zcc3.s` | `0f8398bbed202e4175552f1f2bb35668d6653ff0917de30fb741ac401a8f8df6` | Stage 3 compiler assembly output (byte-identical fixed point) |
| `part4.c` | `462f30d3ee017f54f39f6c4e048ff9d456be9da1db1737853c29b13b943b3b41` | System V codegen & IR eligibility updates (33 lines diff, <50 budget) |
| `compiler_passes.c` | `132567e97f13af53e43396a2d01a91aee1fbe4ff3d80d74e37b719936175baec` | SSA-IR lowering & assembly emission with `%rax/%rdx` support |
| `ir_to_x86.c` | `e65a601e5612e871f073ab6977238a3fbdf9d96eb3970c203332f268cc7acf79` | IR-to-x86 lowering with 16-byte stack slots & dual-return support |
