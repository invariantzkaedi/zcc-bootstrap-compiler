# ZCC Oneirogenesis v2 — Evolution Report

**Generated**: 2026-06-07T10:50:45.603079+00:00

## Summary

| Metric | Value |
|--------|-------|
| Global Generation | 345 |
| Total Survived | 188 |
| Total Rejected | 447 |
| Algorithms Discovered | 146 |
| Blacklisted Patterns | 55 |

## Lineage

| Gen | Island | Hash | Mutations | Δ Score | Timestamp |
|-----|--------|------|-----------|---------|----------|
| G0001 | I0 | `083120c8fb87` | Sweep: remove ALL 206 jmp-to-next-label (branch straightening), Replace cmpq $0,%rax with testq %rax,%rax (+1) | -3047.0 | 2026-04-20T10:37:12 |
| G0002 | I0 | `8067151377b4` | Fuse mov+add → leaq $128(%r12), %rax | -8.3 | 2026-04-20T10:39:34 |
| G0003 | I1 | `892b9626bab7` | Fuse mov+add → leaq $136(%r15), %rax | -8.8 | 2026-04-20T10:39:39 |
| G0004 | I2 | `714632698847` | Fuse mov+add → leaq $144(%r12), %rax | -8.7 | 2026-04-20T10:39:56 |
| G0005 | I4 | `89572fb2aad0` | Fuse mov+add → leaq $136(%r14), %rax | -8.0 | 2026-04-20T10:40:04 |
| G0006 | I0 | `1e795a720819` | Fuse mov+add → leaq $24(%r12), %rax | -8.5 | 2026-04-20T10:40:09 |
| G0007 | I2 | `ff1174be2e6e` | Fuse mov+add → leaq $24(%r12), %rax | -8.3 | 2026-04-20T10:40:19 |
| G0008 | I4 | `64c8e9238f00` | Fuse mov+add → leaq $16(%r14), %rax | -5.5 | 2026-04-20T10:40:42 |
| G0009 | I1 | `0ccda12fe0f2` | Fuse mov+add → leaq $64(%r13), %rax | -7.8 | 2026-04-20T10:40:47 |
| G0010 | I2 | `ef0f40bd951d` | Fuse mov+add → leaq $24(%r14), %rax | -6.2 | 2026-04-20T10:40:52 |
| G0011 | I4 | `be3dbb1e00be` | Fuse mov+add → leaq $24(%r15), %rax | -11.1 | 2026-04-20T10:41:02 |
| G0012 | I3 | `1a7ef4b22287` | Fuse mov+add → leaq $18144(%r15), %rax | -9.1 | 2026-04-20T10:41:18 |
| G0013 | I0 | `fd21e19a14c0` | Fuse mov+add → leaq $192(%rbx), %rax | -9.0 | 2026-04-20T10:41:27 |
| G0014 | I2 | `20aff99dcaa2` | Fuse mov+add → leaq $144(%r12), %rax | -10.8 | 2026-04-20T10:41:37 |
| G0015 | I3 | `66a57eeca264` | Fuse mov+add → leaq $192(%r13), %rax | -7.7 | 2026-04-20T10:41:42 |
| G0016 | I1 | `1d72b376d78d` | Fuse mov+add → leaq $68(%r13), %rax | -10.1 | 2026-04-20T10:41:52 |
| G0017 | I3 | `fc7f50b70ba5` | Fuse mov+add → leaq $128(%r12), %rax | -9.2 | 2026-04-20T10:42:14 |
| G0018 | I4 | `e9ff749d086d` | Remove dead movq to %rax (overwritten at +1) | -8.7 | 2026-04-20T10:42:24 |
| G0019 | I0 | `02251a57ad92` | Fuse mov+add → leaq $192(%r13), %rax | -10.4 | 2026-04-20T10:45:34 |
| G0020 | I1 | `d6c5bac0f4c4` | Fuse mov+add → leaq $136(%r15), %rax | -13.6 | 2026-04-20T10:45:39 |
| G0021 | I3 | `30acda385fe5` | Fuse mov+add → leaq $1160(%r13), %rax, Fuse mov+add → leaq $160(%rbx), %rax | -11.7 | 2026-04-20T10:46:08 |
| G0022 | I0 | `a7762fe9adb1` | Fuse mov+add → leaq $24(%r13), %rax | -9.9 | 2026-04-20T10:46:34 |
| G0023 | I0 | `8861ac7a85b8` | Fuse mov+add → leaq $4(%r15), %rax, Fuse mov+add → leaq $212(%r13), %rax | -17.2 | 2026-04-20T10:46:51 |
| G0024 | I3 | `77bedb1e3bbd` | Fuse mov+add → leaq $17952(%r14), %rax | -9.3 | 2026-04-20T10:47:05 |
| G0025 | I1 | `6aecaaa646d1` | Fuse mov+add → leaq $392(%rbx), %rax | -9.5 | 2026-04-20T10:47:17 |
| G0026 | I0 | `107f4a514d76` | Fuse mov+add → leaq $1160(%r13), %rax | -6.8 | 2026-04-20T10:47:28 |
| G0027 | I1 | `55094656721b` | Fuse mov+add → leaq $16(%r14), %rax | -7.6 | 2026-04-20T10:47:40 |
| G0028 | I3 | `607d0d0880e5` | Fuse mov+add → leaq $192(%r13), %rax | -8.4 | 2026-04-20T10:47:50 |
| G0029 | I3 | `a94d00af9e96` | Fuse mov+add → leaq $352(%r13), %rax | -8.2 | 2026-04-20T10:48:01 |
| G0030 | I0 | `326070e72661` | Fuse mov+add → leaq $17952(%r14), %rax | -10.0 | 2026-04-20T10:48:07 |
| G0031 | I2 | `5f499b7ac2b9` | Fuse mov+add → leaq $216(%r13), %rax | -9.4 | 2026-04-20T10:48:17 |
| G0032 | I0 | `1a3397d80f92` | Fuse mov+add → leaq $200(%r15), %rax | -8.1 | 2026-04-20T10:48:27 |
| G0033 | I0 | `4e7ae2791351` | Fuse mov+add → leaq $136(%r14), %rax | -9.2 | 2026-04-20T10:48:43 |
| G0034 | I1 | `96bf5ed5c3fd` | Fuse mov+add → leaq $16(%r13), %rax | -8.5 | 2026-04-20T10:49:03 |
| G0035 | I0 | `443805d74a21` | Fuse mov+add → leaq $376(%r13), %rax | -24.3 | 2026-04-20T11:17:11 |
| G0036 | I1 | `5112eaed4ff1` | Fuse mov+add → leaq $140(%r12), %rax | -7.8 | 2026-04-20T11:17:17 |
| G0037 | I8 | `273436e9dbf5` | Fuse mov+add → leaq $144(%r12), %rax | -8.1 | 2026-04-20T11:17:49 |
| G0038 | I0 | `be683556ae78` | Fuse mov+add → leaq $160(%r12), %rax | -8.6 | 2026-04-20T11:17:54 |
| G0039 | I1 | `6209b3ab870a` | Fuse mov+add → leaq $8(%r15), %rax | -8.6 | 2026-04-20T11:18:00 |
| G0040 | I6 | `e4821b46e172` | Fuse mov+add → leaq $192(%rbx), %rax | -7.0 | 2026-04-20T11:18:11 |
| G0041 | I7 | `e926080e94b7` | Fuse mov+add → leaq $32(%r14), %rax | -7.4 | 2026-04-20T11:18:18 |
| G0042 | I0 | `7855293a0d97` | Fuse mov+add → leaq $296(%r12), %rax, Fuse mov+add → leaq $72(%r13), %rax | -17.1 | 2026-04-20T11:18:33 |
| G0043 | I1 | `04d6c1c75eec` | Fuse mov+add → leaq $4816896(%r14), %rax | -9.7 | 2026-04-20T11:18:38 |
| G0044 | I6 | `5c499d05b14c` | Fuse mov+add → leaq $128(%rbx), %rax | -4.8 | 2026-04-20T11:18:50 |
| G0045 | I7 | `bb35c6010c5e` | Swap independent pair to reduce WAR pipeline stall | -0.7 | 2026-04-20T11:18:55 |
| G0046 | I2 | `3f59b35a226c` | Fuse mov+add → leaq $136(%r13), %rax | -8.6 | 2026-04-20T11:19:16 |
| G0047 | I8 | `932bb76cef31` | Fuse mov+add → leaq $192(%rbx), %rax | -9.0 | 2026-04-20T11:19:33 |
| G0048 | I1 | `ce9adf8f2f63` | Fuse mov+add → leaq $52(%rbx), %rax | -9.1 | 2026-04-20T11:19:43 |
| G0049 | I3 | `55a8a51a62d0` | Fuse mov+add → leaq $200(%r14), %rax | -8.1 | 2026-04-20T11:19:49 |
| G0050 | I8 | `b142113db2aa` | Fuse mov+add → leaq $18152(%r13), %rax | -8.6 | 2026-04-20T11:20:06 |
| G0051 | I1 | `79e84be400f8` | Swap independent pair to reduce WAR pipeline stall | -0.8 | 2026-04-20T13:33:24 |
| G0052 | I2 | `5b517234647c` | Swap independent pair to reduce WAR pipeline stall, Swap independent pair to reduce WAR pipeline stall | -1.0 | 2026-04-20T13:33:26 |
| G0053 | I4 | `74be373cde3b` | Swap independent pair to reduce WAR pipeline stall | -1.1 | 2026-04-20T13:33:31 |
| G0054 | I5 | `ccaf2268fd8c` | Swap independent pair to reduce WAR pipeline stall, Swap independent pair to reduce WAR pipeline stall | -0.3 | 2026-04-20T13:33:34 |
| G0055 | I7 | `2a753e82f790` | Swap independent pair to reduce WAR pipeline stall | -0.8 | 2026-04-20T13:33:38 |
| G0056 | I9 | `618496c0b7eb` | Swap independent pair to reduce WAR pipeline stall | -0.4 | 2026-04-20T13:33:42 |
| G0057 | I8 | `4ef8cf2f86a2` | Swap independent pair to reduce WAR pipeline stall | -1.2 | 2026-04-20T13:34:03 |
| G0058 | I3 | `7fb9c55cd4ce` | Remove dead movq to %rsi (overwritten at +6), Swap independent pair to reduce WAR pipeline stall | -8.7 | 2026-04-20T13:34:16 |
| G0059 | I5 | `b718f1c7aab0` | Swap independent pair to reduce WAR pipeline stall, Swap independent pair to reduce WAR pipeline stall | -0.1 | 2026-04-20T13:34:20 |
| G0060 | I7 | `6a08f88eca7b` | Remove dead movq to %rax (overwritten at +1), Swap independent pair to reduce WAR pipeline stall | -9.3 | 2026-04-20T13:34:25 |
| G0061 | I9 | `0fa545e859ec` | Remove dead movq to %rax (overwritten at +1) | -10.5 | 2026-04-20T13:34:29 |
| G0062 | I9 | `6c30003453bf` | Swap independent pair to reduce WAR pipeline stall, Swap independent pair to reduce WAR pipeline stall (+1) | -0.4 | 2026-04-20T13:34:52 |
| G0063 | I0 | `c9610461a66c` | Swap independent pair to reduce WAR pipeline stall | -0.7 | 2026-04-20T13:34:54 |
| G0064 | I1 | `20bccff2aaf3` | Remove dead movq to %rax (overwritten at +1) | -8.9 | 2026-04-20T13:34:56 |
| G0065 | I3 | `d701d8db9fdf` | Swap independent pair to reduce WAR pipeline stall | -1.4 | 2026-04-20T13:35:01 |
| G0066 | I4 | `5e8a134650f1` | Swap independent pair to reduce WAR pipeline stall, Remove dead movq to %r11 (overwritten at +7) | -8.2 | 2026-04-20T13:35:03 |
| G0067 | I5 | `37382d1ff0a0` | Remove dead movq to %rax (overwritten at +1) | -9.2 | 2026-04-20T13:35:06 |
| G0068 | I7 | `ea392b49badf` | Remove dead movq to %rax (overwritten at +1), Remove dead movq to %rax (overwritten at +1) | -20.2 | 2026-04-20T13:35:09 |
| G0069 | I1 | `14b6657ce240` | Swap independent pair to reduce WAR pipeline stall | -0.5 | 2026-04-20T13:37:32 |
| G0070 | I3 | `82b4999349b6` | Swap independent pair to reduce WAR pipeline stall | -1.2 | 2026-04-20T13:37:38 |
| G0071 | I4 | `f65c9f88fc07` | Swap independent pair to reduce WAR pipeline stall | -0.0 | 2026-04-20T13:37:42 |
| G0072 | I7 | `6f6b5233e2fb` | Swap independent pair to reduce WAR pipeline stall | -1.0 | 2026-04-20T13:37:48 |
| G0073 | I8 | `eb274878e903` | Swap independent pair to reduce WAR pipeline stall | -1.1 | 2026-04-20T13:37:51 |
| G0074 | I9 | `1bb381ecb444` | Swap independent pair to reduce WAR pipeline stall | -1.2 | 2026-04-20T13:37:54 |
| G0075 | I2 | `dc56d177a337` | Remove dead movq to %rax (overwritten at +1) | -8.2 | 2026-04-20T13:38:02 |
| G0076 | I0 | `3355c0d6d847` | Remove dead movq to %rax (overwritten at +1) | -8.6 | 2026-04-20T13:38:24 |
| G0077 | I2 | `5a9f058f17d4` | Swap independent pair to reduce WAR pipeline stall | -1.6 | 2026-04-20T13:38:29 |
| G0078 | I4 | `22519d8860a9` | Swap independent pair to reduce WAR pipeline stall | -0.5 | 2026-04-20T13:38:35 |
| G0079 | I7 | `0b75639714b7` | Remove dead movq to %rax (overwritten at +1) | -8.9 | 2026-04-20T13:38:41 |
| G0080 | I8 | `5b94002aa603` | Remove dead movq to %rax (overwritten at +1), Swap independent pair to reduce WAR pipeline stall | -9.7 | 2026-04-20T13:38:44 |
| G0081 | I9 | `67e511fa12fc` | Swap independent pair to reduce WAR pipeline stall | -1.7 | 2026-04-20T13:38:47 |
| G0082 | I2 | `bb0b6cb74d9c` | Swap independent pair to reduce WAR pipeline stall | -0.2 | 2026-04-20T13:38:54 |
| G0083 | I5 | `a149f2dc6555` | Swap independent pair to reduce WAR pipeline stall, Swap independent pair to reduce WAR pipeline stall | -0.5 | 2026-04-20T13:39:02 |
| G0084 | I9 | `07a113a26bcf` | Swap independent pair to reduce WAR pipeline stall | -0.2 | 2026-04-20T13:39:11 |
| G0085 | I0 | `2059a750bf89` | Swap independent pair to reduce WAR pipeline stall, Swap independent pair to reduce WAR pipeline stall | -0.1 | 2026-04-20T13:39:14 |
| G0086 | I2 | `6e7c29317156` | Swap independent pair to reduce WAR pipeline stall | -0.0 | 2026-04-20T13:39:19 |
| G0087 | I3 | `fd487d6cb271` | Remove dead movq to %rax (overwritten at +1) | -8.8 | 2026-04-20T13:39:22 |
| G0088 | I7 | `6c20c291d130` | Swap independent pair to reduce WAR pipeline stall | -1.0 | 2026-04-20T13:39:32 |
| G0089 | I9 | `9e08ca7b6562` | Remove dead movq to %rax (overwritten at +1), Remove dead movq to %rax (overwritten at +1) | -18.3 | 2026-04-20T13:39:38 |
| G0090 | I2 | `a7adb6ab159d` | Swap independent pair to reduce WAR pipeline stall | -0.6 | 2026-04-20T13:42:47 |
| G0091 | I3 | `89ebe6bcd128` | Swap independent pair to reduce WAR pipeline stall, Remove dead movq to %rax (overwritten at +1) | -9.3 | 2026-04-20T13:42:51 |
| G0092 | I4 | `f3d2922614d8` | Swap independent pair to reduce WAR pipeline stall | -1.3 | 2026-04-20T13:42:54 |
| G0093 | I3 | `bfcb944199ea` | Swap independent pair to reduce WAR pipeline stall, Swap independent pair to reduce WAR pipeline stall | -1.6 | 2026-04-20T13:43:03 |
| G0094 | I4 | `feec1d72c1b7` | Swap independent pair to reduce WAR pipeline stall | -1.0 | 2026-04-20T13:43:06 |
| G0095 | I2 | `0721d608ff5f` | Swap independent pair to reduce WAR pipeline stall | -0.5 | 2026-04-20T13:43:12 |
| G0096 | I0 | `2820e7bd32b3` | Swap independent pair to reduce WAR pipeline stall | -0.2 | 2026-04-20T13:43:20 |
| G0097 | I1 | `f147794a7158` | Swap independent pair to reduce WAR pipeline stall | -0.3 | 2026-04-20T13:43:23 |
| G0098 | I2 | `8641b8808e23` | Swap independent pair to reduce WAR pipeline stall | -0.0 | 2026-04-20T13:43:26 |
| G0099 | I4 | `d18b50b0ade4` | Swap independent pair to reduce WAR pipeline stall | -0.1 | 2026-04-20T13:43:36 |
| G0100 | I1 | `049e5302759d` | Remove dead movq to %rsi (overwritten at +6) | -9.9 | 2026-04-20T13:43:42 |
| G0101 | I2 | `de6892b3fbe5` | Swap independent pair to reduce WAR pipeline stall | -0.9 | 2026-04-20T13:43:45 |
| G0102 | I4 | `66891ad8a46b` | Remove dead movq to %rax (overwritten at +1), Swap independent pair to reduce WAR pipeline stall | -9.9 | 2026-04-20T13:43:50 |
| G0103 | I0 | `fbbc1aa90912` | Swap independent pair to reduce WAR pipeline stall | -0.7 | 2026-04-20T13:43:52 |
| G0104 | I2 | `0a71abf2c431` | Remove dead movq to %rax (overwritten at +1), Swap independent pair to reduce WAR pipeline stall (+1) | -9.8 | 2026-04-20T13:43:57 |
| G0105 | I1 | `4ea70317efc6` | Swap independent pair to reduce WAR pipeline stall | -0.4 | 2026-04-20T13:44:07 |
| G0106 | I4 | `c655828688f6` | Swap independent pair to reduce WAR pipeline stall | -0.9 | 2026-04-20T13:44:15 |
| G0107 | I1 | `24b69f5cc836` | Remove dead movq to %rax (overwritten at +1) | -8.0 | 2026-04-20T13:44:20 |
| G0108 | I4 | `e683b0c27029` | Remove dead movq to %rax (overwritten at +1), Remove dead movq to %rax (overwritten at +1) | -18.8 | 2026-04-20T13:44:28 |
| G0109 | I1 | `7f3635cdfd33` | Swap independent pair to reduce WAR pipeline stall | -2.2 | 2026-04-20T13:44:33 |
| G0110 | I3 | `001216358dfc` | Swap independent pair to reduce WAR pipeline stall | -0.6 | 2026-04-20T13:44:38 |
| G0111 | I4 | `d0cea6e1487f` | Swap independent pair to reduce WAR pipeline stall | -0.8 | 2026-04-20T13:44:41 |
| G0112 | I2 | `1eb9ce34f276` | Remove dead movq to %rax (overwritten at +1), Swap independent pair to reduce WAR pipeline stall | -8.2 | 2026-04-20T13:44:49 |
| G0113 | I4 | `ac47f71abb72` | Swap independent pair to reduce WAR pipeline stall | -0.1 | 2026-04-20T13:44:55 |
| G0114 | I1 | `673569680d6f` | Swap independent pair to reduce WAR pipeline stall | -0.0 | 2026-04-20T13:45:00 |
| G0115 | I2 | `bbfdfc583800` | Swap independent pair to reduce WAR pipeline stall | -1.7 | 2026-04-20T13:45:02 |
| G0116 | I3 | `5cad0719277d` | Swap independent pair to reduce WAR pipeline stall, Swap independent pair to reduce WAR pipeline stall | -0.3 | 2026-04-20T13:45:05 |
| G0117 | I4 | `6a3dc07fd7ea` | Remove dead movq to %rax (overwritten at +1) | -8.6 | 2026-04-20T13:45:08 |
| G0118 | I4 | `90760274cdaa` | Swap independent pair to reduce WAR pipeline stall, Swap independent pair to reduce WAR pipeline stall | -1.1 | 2026-04-20T13:45:18 |
| G0119 | I0 | `1c7dff1a50ee` | Swap independent pair to reduce WAR pipeline stall | -0.5 | 2026-04-20T13:45:20 |
| G0120 | I2 | `8a3d07208a62` | Remove dead movq to %rax (overwritten at +1) | -10.5 | 2026-04-20T13:45:26 |
| G0121 | I4 | `9ac99bc39d7f` | Remove dead movq to %rax (overwritten at +1), Remove dead movq to %rax (overwritten at +1) (+1) | -19.2 | 2026-04-20T13:45:31 |
| G0122 | I1 | `fb1bfa09ad29` | Swap independent pair to reduce WAR pipeline stall | -0.6 | 2026-04-20T13:45:36 |
| G0123 | I2 | `f43ec3b5c3ee` | Remove dead movq to %rax (overwritten at +1) | -10.0 | 2026-04-20T13:45:38 |
| G0124 | I3 | `a91b6b4c4aba` | Swap independent pair to reduce WAR pipeline stall | -0.2 | 2026-04-20T13:45:40 |
| G0125 | I4 | `6869d3eed1f1` | Swap independent pair to reduce WAR pipeline stall | -0.6 | 2026-04-20T13:45:43 |
| G0126 | I1 | `5b99bdc26912` | Swap independent pair to reduce WAR pipeline stall | -0.1 | 2026-04-20T13:45:48 |
| G0127 | I2 | `dd55e6060c9d` | Remove dead movq to %rax (overwritten at +1) | -10.4 | 2026-04-20T13:45:57 |
| G0128 | I3 | `3737e5a6c255` | Swap independent pair to reduce WAR pipeline stall | -0.4 | 2026-04-20T13:46:06 |
| G0129 | I1 | `b26ccb31785b` | Remove dead movq to %rax (overwritten at +1) | -8.6 | 2026-04-20T13:46:14 |
| G0130 | I4 | `327becb09e8d` | Swap independent pair to reduce WAR pipeline stall | -0.0 | 2026-04-20T13:46:20 |
| G0131 | I4 | `5d660156d299` | Swap independent pair to reduce WAR pipeline stall | -0.8 | 2026-04-20T13:46:31 |
| G0331 | I0 | `1f2192398922` | Sink load of -672(%rbp) to %rbx past independent instruction, Sink load of -39376(%rbp) to %r13 past independent instruction | -98.1 | 2026-06-07T09:43:53 |
| G0332 | I0 | `b2ad7ec3f62c` | Fuse mov+add → leaq $25300(%r12), %rax | -17.0 | 2026-06-07T09:44:22 |
| G0333 | I0 | `3f7ebe5fe95f` | Fuse mov+add → leaq $12(%r14), %rax, Sink load of -39368(%rbp) to %r12 past independent instruction | -3.1 | 2026-06-07T10:08:36 |
| G0334 | I0 | `e17d77733766` | Fuse mov+add → leaq $300(%r12), %rax | -3.7 | 2026-06-07T10:08:59 |
| G0335 | I0 | `23ac07f4ea1f` | Fuse mov+add → leaq $16596(%r13), %rax | -7.3 | 2026-06-07T10:38:57 |
| G0336 | I0 | `a299f45454b8` | Fuse mov+add → leaq $344(%r12), %rax, Fuse mov+add → leaq $270925824(%r14), %rax (+1) | -15.6 | 2026-06-07T10:44:56 |
| G0337 | I0 | `fb407a88210c` | Sink load of -1800(%rbp) to %r14 past independent instruction, Fuse mov+add → leaq $16520(%r14), %rax | -3.8 | 2026-06-07T10:45:25 |
| G0338 | I0 | `cd940c77d3e3` | Sink load of -480(%rbp) to %rbx past independent instruction | -0.8 | 2026-06-07T10:45:52 |
| G0339 | I0 | `62a2bfef06df` | Fuse mov+add → leaq $16532(%r13), %rax, Fuse mov+add → leaq $56(%r13), %rax (+1) | -18.6 | 2026-06-07T10:46:24 |
| G0340 | I0 | `70a33122f072` | Sink load of -920(%rbp) to %r12 past independent instruction | -3.5 | 2026-06-07T10:46:55 |
| G0341 | I0 | `3031a80322d0` | Fuse mov+add → leaq $144(%r12), %rax | -14.1 | 2026-06-07T10:47:24 |
| G0342 | I0 | `90758d93f946` | Fuse mov+add → leaq $192(%r12), %rax, Sink load of -1600(%rbp) to %r13 past independent instruction (+1) | -20.4 | 2026-06-07T10:47:47 |
| G0343 | I0 | `79b676930007` | Sink load of -752(%rbp) to %rbx past independent instruction, Sink load of -600(%rbp) to %r14 past independent instruction | -1.7 | 2026-06-07T10:48:33 |
| G0344 | I0 | `85c17a3a4431` | Remove redundant load: %rax already has -872(%rbp), Fuse mov+add → leaq $552(%r13), %rax (+1) | -5.9 | 2026-06-07T10:50:17 |
| G0345 | I0 | `705b87021d03` | Sink load of -80(%rbp) to %r13 past independent instruction, Sink load of -624(%rbp) to %rbx past independent instruction | -2.4 | 2026-06-07T10:50:45 |

## Discovered Algorithms

- `QAlgo-Dream-G1` → [`QAlgo-Dream-G1.json`](journal/QAlgo-Dream-G1.json)
- `QAlgo-Dream-G2` → [`QAlgo-Dream-G2.json`](journal/QAlgo-Dream-G2.json)
- `QAlgo-Dream-G3` → [`QAlgo-Dream-G3.json`](journal/QAlgo-Dream-G3.json)
- `QAlgo-Dream-G4` → [`QAlgo-Dream-G4.json`](journal/QAlgo-Dream-G4.json)
- `QAlgo-Dream-G5` → [`QAlgo-Dream-G5.json`](journal/QAlgo-Dream-G5.json)
- `QAlgo-Dream-G6` → [`QAlgo-Dream-G6.json`](journal/QAlgo-Dream-G6.json)
- `QAlgo-Dream-G7` → [`QAlgo-Dream-G7.json`](journal/QAlgo-Dream-G7.json)
- `QAlgo-Dream-G8` → [`QAlgo-Dream-G8.json`](journal/QAlgo-Dream-G8.json)
- `QAlgo-Dream-G9` → [`QAlgo-Dream-G9.json`](journal/QAlgo-Dream-G9.json)
- `QAlgo-Dream-G10` → [`QAlgo-Dream-G10.json`](journal/QAlgo-Dream-G10.json)
- `QAlgo-Dream-G11` → [`QAlgo-Dream-G11.json`](journal/QAlgo-Dream-G11.json)
- `QAlgo-Dream-G12` → [`QAlgo-Dream-G12.json`](journal/QAlgo-Dream-G12.json)
- `QAlgo-Dream-G13` → [`QAlgo-Dream-G13.json`](journal/QAlgo-Dream-G13.json)
- `QAlgo-Dream-G14` → [`QAlgo-Dream-G14.json`](journal/QAlgo-Dream-G14.json)
- `QAlgo-Dream-G15` → [`QAlgo-Dream-G15.json`](journal/QAlgo-Dream-G15.json)
- `QAlgo-Dream-G16` → [`QAlgo-Dream-G16.json`](journal/QAlgo-Dream-G16.json)
- `QAlgo-Dream-G17` → [`QAlgo-Dream-G17.json`](journal/QAlgo-Dream-G17.json)
- `QAlgo-Dream-G18` → [`QAlgo-Dream-G18.json`](journal/QAlgo-Dream-G18.json)
- `QAlgo-Dream-G19` → [`QAlgo-Dream-G19.json`](journal/QAlgo-Dream-G19.json)
- `QAlgo-Dream-G20` → [`QAlgo-Dream-G20.json`](journal/QAlgo-Dream-G20.json)
- `QAlgo-Dream-G21` → [`QAlgo-Dream-G21.json`](journal/QAlgo-Dream-G21.json)
- `QAlgo-Dream-G22` → [`QAlgo-Dream-G22.json`](journal/QAlgo-Dream-G22.json)
- `QAlgo-Dream-G23` → [`QAlgo-Dream-G23.json`](journal/QAlgo-Dream-G23.json)
- `QAlgo-Dream-G24` → [`QAlgo-Dream-G24.json`](journal/QAlgo-Dream-G24.json)
- `QAlgo-Dream-G25` → [`QAlgo-Dream-G25.json`](journal/QAlgo-Dream-G25.json)
- `QAlgo-Dream-G26` → [`QAlgo-Dream-G26.json`](journal/QAlgo-Dream-G26.json)
- `QAlgo-Dream-G27` → [`QAlgo-Dream-G27.json`](journal/QAlgo-Dream-G27.json)
- `QAlgo-Dream-G28` → [`QAlgo-Dream-G28.json`](journal/QAlgo-Dream-G28.json)
- `QAlgo-Dream-G29` → [`QAlgo-Dream-G29.json`](journal/QAlgo-Dream-G29.json)
- `QAlgo-Dream-G30` → [`QAlgo-Dream-G30.json`](journal/QAlgo-Dream-G30.json)
- `QAlgo-Dream-G31` → [`QAlgo-Dream-G31.json`](journal/QAlgo-Dream-G31.json)
- `QAlgo-Dream-G32` → [`QAlgo-Dream-G32.json`](journal/QAlgo-Dream-G32.json)
- `QAlgo-Dream-G33` → [`QAlgo-Dream-G33.json`](journal/QAlgo-Dream-G33.json)
- `QAlgo-Dream-G34` → [`QAlgo-Dream-G34.json`](journal/QAlgo-Dream-G34.json)
- `QAlgo-Dream-G35` → [`QAlgo-Dream-G35.json`](journal/QAlgo-Dream-G35.json)
- `QAlgo-Dream-G36` → [`QAlgo-Dream-G36.json`](journal/QAlgo-Dream-G36.json)
- `QAlgo-Dream-G37` → [`QAlgo-Dream-G37.json`](journal/QAlgo-Dream-G37.json)
- `QAlgo-Dream-G38` → [`QAlgo-Dream-G38.json`](journal/QAlgo-Dream-G38.json)
- `QAlgo-Dream-G39` → [`QAlgo-Dream-G39.json`](journal/QAlgo-Dream-G39.json)
- `QAlgo-Dream-G40` → [`QAlgo-Dream-G40.json`](journal/QAlgo-Dream-G40.json)
- `QAlgo-Dream-G41` → [`QAlgo-Dream-G41.json`](journal/QAlgo-Dream-G41.json)
- `QAlgo-Dream-G42` → [`QAlgo-Dream-G42.json`](journal/QAlgo-Dream-G42.json)
- `QAlgo-Dream-G43` → [`QAlgo-Dream-G43.json`](journal/QAlgo-Dream-G43.json)
- `QAlgo-Dream-G44` → [`QAlgo-Dream-G44.json`](journal/QAlgo-Dream-G44.json)
- `QAlgo-Dream-G45` → [`QAlgo-Dream-G45.json`](journal/QAlgo-Dream-G45.json)
- `QAlgo-Dream-G46` → [`QAlgo-Dream-G46.json`](journal/QAlgo-Dream-G46.json)
- `QAlgo-Dream-G47` → [`QAlgo-Dream-G47.json`](journal/QAlgo-Dream-G47.json)
- `QAlgo-Dream-G48` → [`QAlgo-Dream-G48.json`](journal/QAlgo-Dream-G48.json)
- `QAlgo-Dream-G49` → [`QAlgo-Dream-G49.json`](journal/QAlgo-Dream-G49.json)
- `QAlgo-Dream-G50` → [`QAlgo-Dream-G50.json`](journal/QAlgo-Dream-G50.json)
- `QAlgo-Dream-G51` → [`QAlgo-Dream-G51.json`](journal/QAlgo-Dream-G51.json)
- `QAlgo-Dream-G52` → [`QAlgo-Dream-G52.json`](journal/QAlgo-Dream-G52.json)
- `QAlgo-Dream-G53` → [`QAlgo-Dream-G53.json`](journal/QAlgo-Dream-G53.json)
- `QAlgo-Dream-G54` → [`QAlgo-Dream-G54.json`](journal/QAlgo-Dream-G54.json)
- `QAlgo-Dream-G55` → [`QAlgo-Dream-G55.json`](journal/QAlgo-Dream-G55.json)
- `QAlgo-Dream-G56` → [`QAlgo-Dream-G56.json`](journal/QAlgo-Dream-G56.json)
- `QAlgo-Dream-G57` → [`QAlgo-Dream-G57.json`](journal/QAlgo-Dream-G57.json)
- `QAlgo-Dream-G58` → [`QAlgo-Dream-G58.json`](journal/QAlgo-Dream-G58.json)
- `QAlgo-Dream-G59` → [`QAlgo-Dream-G59.json`](journal/QAlgo-Dream-G59.json)
- `QAlgo-Dream-G60` → [`QAlgo-Dream-G60.json`](journal/QAlgo-Dream-G60.json)
- `QAlgo-Dream-G61` → [`QAlgo-Dream-G61.json`](journal/QAlgo-Dream-G61.json)
- `QAlgo-Dream-G62` → [`QAlgo-Dream-G62.json`](journal/QAlgo-Dream-G62.json)
- `QAlgo-Dream-G63` → [`QAlgo-Dream-G63.json`](journal/QAlgo-Dream-G63.json)
- `QAlgo-Dream-G64` → [`QAlgo-Dream-G64.json`](journal/QAlgo-Dream-G64.json)
- `QAlgo-Dream-G65` → [`QAlgo-Dream-G65.json`](journal/QAlgo-Dream-G65.json)
- `QAlgo-Dream-G66` → [`QAlgo-Dream-G66.json`](journal/QAlgo-Dream-G66.json)
- `QAlgo-Dream-G67` → [`QAlgo-Dream-G67.json`](journal/QAlgo-Dream-G67.json)
- `QAlgo-Dream-G68` → [`QAlgo-Dream-G68.json`](journal/QAlgo-Dream-G68.json)
- `QAlgo-Dream-G69` → [`QAlgo-Dream-G69.json`](journal/QAlgo-Dream-G69.json)
- `QAlgo-Dream-G70` → [`QAlgo-Dream-G70.json`](journal/QAlgo-Dream-G70.json)
- `QAlgo-Dream-G71` → [`QAlgo-Dream-G71.json`](journal/QAlgo-Dream-G71.json)
- `QAlgo-Dream-G72` → [`QAlgo-Dream-G72.json`](journal/QAlgo-Dream-G72.json)
- `QAlgo-Dream-G73` → [`QAlgo-Dream-G73.json`](journal/QAlgo-Dream-G73.json)
- `QAlgo-Dream-G74` → [`QAlgo-Dream-G74.json`](journal/QAlgo-Dream-G74.json)
- `QAlgo-Dream-G75` → [`QAlgo-Dream-G75.json`](journal/QAlgo-Dream-G75.json)
- `QAlgo-Dream-G76` → [`QAlgo-Dream-G76.json`](journal/QAlgo-Dream-G76.json)
- `QAlgo-Dream-G77` → [`QAlgo-Dream-G77.json`](journal/QAlgo-Dream-G77.json)
- `QAlgo-Dream-G78` → [`QAlgo-Dream-G78.json`](journal/QAlgo-Dream-G78.json)
- `QAlgo-Dream-G79` → [`QAlgo-Dream-G79.json`](journal/QAlgo-Dream-G79.json)
- `QAlgo-Dream-G80` → [`QAlgo-Dream-G80.json`](journal/QAlgo-Dream-G80.json)
- `QAlgo-Dream-G81` → [`QAlgo-Dream-G81.json`](journal/QAlgo-Dream-G81.json)
- `QAlgo-Dream-G82` → [`QAlgo-Dream-G82.json`](journal/QAlgo-Dream-G82.json)
- `QAlgo-Dream-G83` → [`QAlgo-Dream-G83.json`](journal/QAlgo-Dream-G83.json)
- `QAlgo-Dream-G84` → [`QAlgo-Dream-G84.json`](journal/QAlgo-Dream-G84.json)
- `QAlgo-Dream-G85` → [`QAlgo-Dream-G85.json`](journal/QAlgo-Dream-G85.json)
- `QAlgo-Dream-G86` → [`QAlgo-Dream-G86.json`](journal/QAlgo-Dream-G86.json)
- `QAlgo-Dream-G87` → [`QAlgo-Dream-G87.json`](journal/QAlgo-Dream-G87.json)
- `QAlgo-Dream-G88` → [`QAlgo-Dream-G88.json`](journal/QAlgo-Dream-G88.json)
- `QAlgo-Dream-G89` → [`QAlgo-Dream-G89.json`](journal/QAlgo-Dream-G89.json)
- `QAlgo-Dream-G90` → [`QAlgo-Dream-G90.json`](journal/QAlgo-Dream-G90.json)
- `QAlgo-Dream-G91` → [`QAlgo-Dream-G91.json`](journal/QAlgo-Dream-G91.json)
- `QAlgo-Dream-G92` → [`QAlgo-Dream-G92.json`](journal/QAlgo-Dream-G92.json)
- `QAlgo-Dream-G93` → [`QAlgo-Dream-G93.json`](journal/QAlgo-Dream-G93.json)
- `QAlgo-Dream-G94` → [`QAlgo-Dream-G94.json`](journal/QAlgo-Dream-G94.json)
- `QAlgo-Dream-G95` → [`QAlgo-Dream-G95.json`](journal/QAlgo-Dream-G95.json)
- `QAlgo-Dream-G96` → [`QAlgo-Dream-G96.json`](journal/QAlgo-Dream-G96.json)
- `QAlgo-Dream-G97` → [`QAlgo-Dream-G97.json`](journal/QAlgo-Dream-G97.json)
- `QAlgo-Dream-G98` → [`QAlgo-Dream-G98.json`](journal/QAlgo-Dream-G98.json)
- `QAlgo-Dream-G99` → [`QAlgo-Dream-G99.json`](journal/QAlgo-Dream-G99.json)
- `QAlgo-Dream-G100` → [`QAlgo-Dream-G100.json`](journal/QAlgo-Dream-G100.json)
- `QAlgo-Dream-G101` → [`QAlgo-Dream-G101.json`](journal/QAlgo-Dream-G101.json)
- `QAlgo-Dream-G102` → [`QAlgo-Dream-G102.json`](journal/QAlgo-Dream-G102.json)
- `QAlgo-Dream-G103` → [`QAlgo-Dream-G103.json`](journal/QAlgo-Dream-G103.json)
- `QAlgo-Dream-G104` → [`QAlgo-Dream-G104.json`](journal/QAlgo-Dream-G104.json)
- `QAlgo-Dream-G105` → [`QAlgo-Dream-G105.json`](journal/QAlgo-Dream-G105.json)
- `QAlgo-Dream-G106` → [`QAlgo-Dream-G106.json`](journal/QAlgo-Dream-G106.json)
- `QAlgo-Dream-G107` → [`QAlgo-Dream-G107.json`](journal/QAlgo-Dream-G107.json)
- `QAlgo-Dream-G108` → [`QAlgo-Dream-G108.json`](journal/QAlgo-Dream-G108.json)
- `QAlgo-Dream-G109` → [`QAlgo-Dream-G109.json`](journal/QAlgo-Dream-G109.json)
- `QAlgo-Dream-G110` → [`QAlgo-Dream-G110.json`](journal/QAlgo-Dream-G110.json)
- `QAlgo-Dream-G111` → [`QAlgo-Dream-G111.json`](journal/QAlgo-Dream-G111.json)
- `QAlgo-Dream-G112` → [`QAlgo-Dream-G112.json`](journal/QAlgo-Dream-G112.json)
- `QAlgo-Dream-G113` → [`QAlgo-Dream-G113.json`](journal/QAlgo-Dream-G113.json)
- `QAlgo-Dream-G114` → [`QAlgo-Dream-G114.json`](journal/QAlgo-Dream-G114.json)
- `QAlgo-Dream-G115` → [`QAlgo-Dream-G115.json`](journal/QAlgo-Dream-G115.json)
- `QAlgo-Dream-G116` → [`QAlgo-Dream-G116.json`](journal/QAlgo-Dream-G116.json)
- `QAlgo-Dream-G117` → [`QAlgo-Dream-G117.json`](journal/QAlgo-Dream-G117.json)
- `QAlgo-Dream-G118` → [`QAlgo-Dream-G118.json`](journal/QAlgo-Dream-G118.json)
- `QAlgo-Dream-G119` → [`QAlgo-Dream-G119.json`](journal/QAlgo-Dream-G119.json)
- `QAlgo-Dream-G120` → [`QAlgo-Dream-G120.json`](journal/QAlgo-Dream-G120.json)
- `QAlgo-Dream-G121` → [`QAlgo-Dream-G121.json`](journal/QAlgo-Dream-G121.json)
- `QAlgo-Dream-G122` → [`QAlgo-Dream-G122.json`](journal/QAlgo-Dream-G122.json)
- `QAlgo-Dream-G123` → [`QAlgo-Dream-G123.json`](journal/QAlgo-Dream-G123.json)
- `QAlgo-Dream-G124` → [`QAlgo-Dream-G124.json`](journal/QAlgo-Dream-G124.json)
- `QAlgo-Dream-G125` → [`QAlgo-Dream-G125.json`](journal/QAlgo-Dream-G125.json)
- `QAlgo-Dream-G126` → [`QAlgo-Dream-G126.json`](journal/QAlgo-Dream-G126.json)
- `QAlgo-Dream-G127` → [`QAlgo-Dream-G127.json`](journal/QAlgo-Dream-G127.json)
- `QAlgo-Dream-G128` → [`QAlgo-Dream-G128.json`](journal/QAlgo-Dream-G128.json)
- `QAlgo-Dream-G129` → [`QAlgo-Dream-G129.json`](journal/QAlgo-Dream-G129.json)
- `QAlgo-Dream-G130` → [`QAlgo-Dream-G130.json`](journal/QAlgo-Dream-G130.json)
- `QAlgo-Dream-G131` → [`QAlgo-Dream-G131.json`](journal/QAlgo-Dream-G131.json)
- `QAlgo-Dream-G331` → [`QAlgo-Dream-G331.json`](journal/QAlgo-Dream-G331.json)
- `QAlgo-Dream-G332` → [`QAlgo-Dream-G332.json`](journal/QAlgo-Dream-G332.json)
- `QAlgo-Dream-G333` → [`QAlgo-Dream-G333.json`](journal/QAlgo-Dream-G333.json)
- `QAlgo-Dream-G334` → [`QAlgo-Dream-G334.json`](journal/QAlgo-Dream-G334.json)
- `QAlgo-Dream-G335` → [`QAlgo-Dream-G335.json`](journal/QAlgo-Dream-G335.json)
- `QAlgo-Dream-G336` → [`QAlgo-Dream-G336.json`](journal/QAlgo-Dream-G336.json)
- `QAlgo-Dream-G337` → [`QAlgo-Dream-G337.json`](journal/QAlgo-Dream-G337.json)
- `QAlgo-Dream-G338` → [`QAlgo-Dream-G338.json`](journal/QAlgo-Dream-G338.json)
- `QAlgo-Dream-G339` → [`QAlgo-Dream-G339.json`](journal/QAlgo-Dream-G339.json)
- `QAlgo-Dream-G340` → [`QAlgo-Dream-G340.json`](journal/QAlgo-Dream-G340.json)
- `QAlgo-Dream-G341` → [`QAlgo-Dream-G341.json`](journal/QAlgo-Dream-G341.json)
- `QAlgo-Dream-G342` → [`QAlgo-Dream-G342.json`](journal/QAlgo-Dream-G342.json)
- `QAlgo-Dream-G343` → [`QAlgo-Dream-G343.json`](journal/QAlgo-Dream-G343.json)
- `QAlgo-Dream-G344` → [`QAlgo-Dream-G344.json`](journal/QAlgo-Dream-G344.json)
- `QAlgo-Dream-G345` → [`QAlgo-Dream-G345.json`](journal/QAlgo-Dream-G345.json)

## Fitness History

```
G0118 I4: score=1257379
G0119 I0: score=1257418
G0120 I2: score=1257388
G0121 I4: score=1257360
G0122 I1: score=1257398
G0123 I2: score=1257378
G0124 I3: score=1257408
G0125 I4: score=1257359
G0126 I1: score=1257398
G0127 I2: score=1257368
G0128 I3: score=1257408
G0129 I1: score=1257389
G0130 I4: score=1257359
G0131 I4: score=1257358
G0330 I0: score=2240050
G0331 I0: score=2278351
G0332 I0: score=2278334
G0333 I0: score=2278331
G0334 I0: score=2278327
G0335 I0: score=2278320
G0336 I0: score=2278304
G0337 I0: score=2278300
G0338 I0: score=2278300
G0339 I0: score=2278281
G0340 I0: score=2278277
G0341 I0: score=2278263
G0342 I0: score=2278243
G0343 I0: score=2278241
G0344 I0: score=2278235
G0345 I0: score=2278233
```
