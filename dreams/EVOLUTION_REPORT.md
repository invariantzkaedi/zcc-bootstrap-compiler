# ZCC Oneirogenesis v2 — Evolution Report

**Generated**: 2026-06-04T21:07:21.255745+00:00

## Summary

| Metric | Value |
|--------|-------|
| Global Generation | 330 |
| Total Survived | 579 |
| Total Rejected | 607 |
| Algorithms Discovered | 330 |
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
| G0132 | I0 | `f1de63099e27` | Remove unused frame-save: %rax → -56(%rbp) (slot never loaded), Remove unused frame-save: %rax → -592(%rbp) (slot never loaded) | -26.1 | 2026-06-01T18:19:11 |
| G0133 | I1 | `0e6a0cc13a82` | Remove unused frame-save: %rax → -64(%rbp) (slot never loaded) | -13.6 | 2026-06-01T18:19:33 |
| G0134 | I0 | `55ebf57136a2` | Remove unused frame-save: %rax → -72(%rbp) (slot never loaded) | -1.8 | 2026-06-01T18:20:22 |
| G0135 | I1 | `e664c3f34183` | Remove unused frame-save: %rax → -360(%rbp) (slot never loaded) | -12.2 | 2026-06-01T18:20:46 |
| G0136 | I0 | `6e07a5de5ba4` | Sweep: remove ALL 396 jmp-to-next-label (branch straightening), Remove unused frame-save: %rax → -64(%rbp) (slot never loaded) (+1) | -5938.2 | 2026-06-01T18:21:30 |
| G0137 | I1 | `f60c496fd176` | Sweep: remove ALL 396 jmp-to-next-label (branch straightening), Remove unused frame-save: %rax → -384(%rbp) (slot never loaded) (+2) | -5936.5 | 2026-06-01T18:21:51 |
| G0138 | I0 | `0df69ccd9e18` | Remove unused frame-save: %rax → -520(%rbp) (slot never loaded) | -0.7 | 2026-06-01T18:22:31 |
| G0139 | I1 | `9107064343a7` | Remove unused frame-save: %rax → -1368(%rbp) (slot never loaded) | -15.3 | 2026-06-01T18:22:51 |
| G0140 | I2 | `e9f2d06922ae` | Sweep: remove ALL 396 jmp-to-next-label (branch straightening), Remove unused frame-save: %rax → -64(%rbp) (slot never loaded) | -5914.7 | 2026-06-01T18:23:10 |
| G0141 | I1 | `657e9c95ef46` | Remove unused frame-save: %rax → -1184(%rbp) (slot never loaded) | -14.5 | 2026-06-01T18:23:53 |
| G0142 | I0 | `b585124c8375` | Remove unused frame-save: %rax → -456(%rbp) (slot never loaded), Remove unused frame-save: %rax → -17168(%rbp) (slot never loaded) | -35.5 | 2026-06-01T18:24:33 |
| G0143 | I1 | `22ab4d75d36e` | Remove unused frame-save: %rax → -440(%rbp) (slot never loaded) | -9.2 | 2026-06-01T18:24:53 |
| G0144 | I2 | `6b32954e032f` | Remove unused frame-save: %rax → -624(%rbp) (slot never loaded) | -19.1 | 2026-06-01T18:25:12 |
| G0145 | I1 | `d68217060ec7` | Remove unused frame-save: %rax → -592(%rbp) (slot never loaded), Remove unused frame-save: %rax → -504(%rbp) (slot never loaded) | -24.1 | 2026-06-01T18:25:31 |
| G0146 | I0 | `83d2a630ba97` | Remove unused frame-save: %rax → -440(%rbp) (slot never loaded) | -14.8 | 2026-06-01T18:26:10 |
| G0147 | I1 | `615b04db3c6f` | Remove unused frame-save: %rax → -1280(%rbp) (slot never loaded) | -16.4 | 2026-06-01T18:26:28 |
| G0148 | I2 | `7ae9a589f81a` | Remove unused frame-save: %rax → -64(%rbp) (slot never loaded) | -9.3 | 2026-06-01T18:26:47 |
| G0149 | I2 | `2da04c740186` | Remove unused frame-save: %rax → -560(%rbp) (slot never loaded) | -12.2 | 2026-06-01T18:27:41 |
| G0150 | I2 | `52056e2559b4` | Remove unused frame-save: %rax → -520(%rbp) (slot never loaded) | -11.5 | 2026-06-01T18:28:02 |
| G0151 | I0 | `79ec1d5358a8` | Remove unused frame-save: %rax → -1368(%rbp) (slot never loaded) | -11.3 | 2026-06-01T18:28:22 |
| G0152 | I2 | `43fdef6f417a` | Remove unused frame-save: %rax → -464(%rbp) (slot never loaded) | -7.8 | 2026-06-01T18:29:22 |
| G0153 | I1 | `25c38694eb1a` | Remove unused frame-save: %rax → -72(%rbp) (slot never loaded) | -7.9 | 2026-06-01T18:30:01 |
| G0154 | I2 | `a044759a15e7` | Remove unused frame-save: %rax → -17168(%rbp) (slot never loaded) | -14.6 | 2026-06-01T18:30:21 |
| G0155 | I0 | `4c194dea22c1` | Remove unused frame-save: %rax → -504(%rbp) (slot never loaded) | -8.6 | 2026-06-01T18:30:40 |
| G0156 | I2 | `aed37add75d1` | Remove unused frame-save: %rax → -456(%rbp) (slot never loaded) | -14.5 | 2026-06-01T18:31:20 |
| G0157 | I2 | `f3a389afe538` | Remove unused frame-save: %rax → -1280(%rbp) (slot never loaded) | -10.7 | 2026-06-01T18:32:16 |
| G0158 | I0 | `cad97f45818a` | Remove unused frame-save: %rax → -1280(%rbp) (slot never loaded) | -16.2 | 2026-06-01T18:32:35 |
| G0159 | I1 | `b8b31c5268f6` | Remove unused frame-save: %rax → -376(%rbp) (slot never loaded) | -11.6 | 2026-06-01T18:32:54 |
| G0160 | I2 | `8e1b4bbb865b` | Remove unused frame-save: %rax → -64(%rbp) (slot never loaded) | -11.9 | 2026-06-01T18:33:12 |
| G0161 | I2 | `9bc23e960157` | Remove unused frame-save: %rax → -1184(%rbp) (slot never loaded) | -12.6 | 2026-06-01T18:33:35 |
| G0162 | I1 | `a7be926e2880` | Remove unused frame-save: %rax → -464(%rbp) (slot never loaded) | -10.4 | 2026-06-01T18:33:56 |
| G0163 | I2 | `0fa9b1adf52c` | Remove unused frame-save: %rax → -1368(%rbp) (slot never loaded) | -10.1 | 2026-06-01T18:34:27 |
| G0164 | I1 | `6367dbefd293` | Remove unused frame-save: %rax → -1136(%rbp) (slot never loaded) | -16.4 | 2026-06-01T18:35:04 |
| G0165 | I2 | `07cb27472371` | Remove unused frame-save: %rax → -72(%rbp) (slot never loaded) | -12.5 | 2026-06-01T18:35:23 |
| G0166 | I0 | `fb25e394c9a8` | Remove unused frame-save: %rax → -560(%rbp) (slot never loaded) | -11.1 | 2026-06-01T18:35:41 |
| G0167 | I2 | `f0420f9926b1` | Remove unused frame-save: %rax → -504(%rbp) (slot never loaded) | -13.6 | 2026-06-01T18:36:18 |
| G0168 | I1 | `bb88b3f6d57c` | Remove unused frame-save: %rax → -456(%rbp) (slot never loaded) | -9.7 | 2026-06-01T18:36:56 |
| G0169 | I1 | `e7ed4414b478` | Remove unused frame-save: %rax → -384(%rbp) (slot never loaded) | -10.8 | 2026-06-01T18:37:58 |
| G0170 | I0 | `45327ad238bb` | Remove unused frame-save: %rax → -376(%rbp) (slot never loaded) | -1.8 | 2026-06-01T18:38:19 |
| G0171 | I1 | `6ea0bf7a385f` | Remove unused frame-save: %rax → -560(%rbp) (slot never loaded) | -14.0 | 2026-06-01T18:38:44 |
| G0172 | I2 | `a0cf168c1c4a` | Remove unused frame-save: %rax → -72(%rbp) (slot never loaded) | -6.6 | 2026-06-01T18:39:06 |
| G0173 | I0 | `93f0b52cd4a0` | Remove unused frame-save: %rax → -624(%rbp) (slot never loaded) | -20.1 | 2026-06-01T18:39:28 |
| G0174 | I1 | `538a5422c6e8` | Remove unused frame-save: %rax → -520(%rbp) (slot never loaded) | -12.8 | 2026-06-01T18:39:50 |
| G0175 | I0 | `f806ef785ae7` | Remove unused frame-save: %rax → -464(%rbp) (slot never loaded) | -7.8 | 2026-06-01T18:40:10 |
| G0176 | I2 | `58cca0bdccda` | Remove unused frame-save: %rax → -440(%rbp) (slot never loaded) | -16.3 | 2026-06-01T18:40:29 |
| G0177 | I1 | `9de2aa911e4a` | Remove unused frame-save: %rax → -17168(%rbp) (slot never loaded) | -12.1 | 2026-06-01T18:40:49 |
| G0178 | I0 | `41e7ad38ea08` | Remove unused frame-save: %rax → -1184(%rbp) (slot never loaded) | -18.0 | 2026-06-01T18:41:13 |
| G0179 | I0 | `4164cc80a913` | Remove unused frame-save: %rax → -17168(%rbp) (slot never loaded) | -5.4 | 2026-06-02T07:17:50 |
| G0180 | I1 | `4164cc80a913` | Remove unused frame-save: %rax → -17168(%rbp) (slot never loaded) | -20.3 | 2026-06-02T07:18:12 |
| G0181 | I0 | `b72bd343f63a` | Replace cmpq $0,%rax with testq %rax,%rax, Replace cmpq $0,%rax with testq %rax,%rax | -9.3 | 2026-06-02T08:30:57 |
| G0182 | I2 | `1ceebffde2e5` | Replace cmpq $0,%rax with testq %rax,%rax, Replace cmpq $0,%rax with testq %rax,%rax | -5.1 | 2026-06-02T08:31:43 |
| G0183 | I0 | `4022d7b27afe` | Replace cmpq $0,%rax with testq %rax,%rax | -1.9 | 2026-06-02T08:32:05 |
| G0184 | I1 | `d8e286913967` | Replace cmpq $0,%rax with testq %rax,%rax | -3.5 | 2026-06-02T08:32:25 |
| G0185 | I0 | `b72bd343f63a` | Replace cmpq $0,%rax with testq %rax,%rax, Replace cmpq $0,%rax with testq %rax,%rax | -7.6 | 2026-06-02T08:34:36 |
| G0186 | I1 | `d0777c523400` | Replace cmpq $0,%rax with testq %rax,%rax | -8.5 | 2026-06-02T08:35:01 |
| G0187 | I2 | `fa12589d2b9c` | Replace cmpq $0,%rax with testq %rax,%rax, Replace cmpq $0,%rax with testq %rax,%rax | -31.4 | 2026-06-02T08:35:27 |
| G0188 | I2 | `5fc1c930fc0a` | Replace cmpq $0,%rax with testq %rax,%rax | -1.4 | 2026-06-02T08:36:36 |
| G0189 | I0 | `dff3c423dbee` | Replace cmpq $0,%rax with testq %rax,%rax, Replace cmpq $0,%rax with testq %rax,%rax | -16.2 | 2026-06-02T08:36:58 |
| G0190 | I1 | `d0ae88870d4f` | Swap independent pair to reduce WAR pipeline stall, Replace cmpq $0,%rax with testq %rax,%rax (+1) | -10.6 | 2026-06-02T08:37:19 |
| G0191 | I2 | `ed9761a65823` | Replace cmpq $0,%rax with testq %rax,%rax, Fuse mov+add → leaq $140(%r12), %rax (+1) | -11.7 | 2026-06-02T08:38:49 |
| G0192 | I1 | `df4b3355eee7` | Replace cmpq $0,%rax with testq %rax,%rax, Fuse mov+add → leaq $216(%r14), %rax | -2.6 | 2026-06-02T08:39:37 |
| G0193 | I0 | `e961a89a9cc2` | Fuse mov+add → leaq $24(%r14), %rax, Fuse mov+add → leaq $6849216(%r12), %rax | -11.7 | 2026-06-02T08:40:22 |
| G0194 | I1 | `32669b811be3` | Fuse mov+add → leaq $52(%r14), %rax, Fuse mov+add → leaq $24(%r12), %rax (+1) | -17.5 | 2026-06-02T08:40:45 |
| G0195 | I0 | `e9d8b2a97305` | Fuse mov+add → leaq $56(%rbx), %rax, Replace cmpq $0,%rax with testq %rax,%rax (+1) | -1.5 | 2026-06-02T08:43:51 |
| G0196 | I1 | `7fa3beac1e04` | Fuse mov+add → leaq $24(%r12), %rax | -0.3 | 2026-06-02T08:44:17 |
| G0197 | I0 | `d008daf78322` | Replace cmpq $0,%rax with testq %rax,%rax | -2.0 | 2026-06-02T08:45:02 |
| G0198 | I1 | `57b233ff0c2d` | Replace cmpq $0,%rax with testq %rax,%rax, Replace cmpq $0,%rax with testq %rax,%rax | -6.2 | 2026-06-02T08:45:25 |
| G0199 | I2 | `fdb7c60edd88` | Replace cmpq $0,%rax with testq %rax,%rax, Replace cmpq $0,%rax with testq %rax,%rax (+1) | -0.1 | 2026-06-02T08:46:50 |
| G0200 | I0 | `57f2212b6495` | Fuse mov+add → leaq $16520(%r14), %rax, Replace cmpq $0,%rax with testq %rax,%rax (+1) | -14.2 | 2026-06-02T08:47:10 |
| G0201 | I1 | `730ce903129b` | Replace cmpq $0,%rax with testq %rax,%rax, Replace cmpq $0,%rax with testq %rax,%rax | -1.6 | 2026-06-02T08:47:34 |
| G0202 | I2 | `5b5e56c652ed` | Replace cmpq $0,%rax with testq %rax,%rax, Fuse mov+add → leaq $128(%r12), %rax (+1) | -10.1 | 2026-06-02T08:50:14 |
| G0203 | I2 | `24c1a694d557` | Fuse mov+add → leaq $52(%r13), %rax | -10.3 | 2026-06-02T08:51:22 |
| G0204 | I1 | `eb53573a088f` | Fuse mov+add → leaq $16520(%r13), %rax, Fuse mov+add → leaq $128(%r15), %rax (+1) | -15.5 | 2026-06-02T08:53:13 |
| G0205 | I0 | `07d11e8b41f0` | Fuse mov+add → leaq $136(%r13), %rax, Replace cmpq $0,%rax with testq %rax,%rax (+1) | -4.6 | 2026-06-02T08:53:55 |
| G0206 | I2 | `e37c556fb3f8` | Replace cmpq $0,%rax with testq %rax,%rax | -2.4 | 2026-06-02T08:54:37 |
| G0207 | I0 | `036699ff137e` | Replace cmpq $0,%rax with testq %rax,%rax, Fuse mov+add → leaq $24(%r12), %rax | -3.4 | 2026-06-02T08:57:11 |
| G0208 | I2 | `b1a5f0c2d521` | Fuse mov+add → leaq $16528(%r13), %rax | -7.5 | 2026-06-02T08:57:54 |
| G0209 | I0 | `daa4666c491f` | Fuse mov+add → leaq $18344(%r12), %rax | -13.7 | 2026-06-02T08:58:17 |
| G0210 | I2 | `c7670aff6018` | Replace cmpq $0,%rax with testq %rax,%rax, Fuse mov+add → leaq $272(%r12), %rax (+1) | -3.0 | 2026-06-02T08:59:02 |
| G0211 | I2 | `d6e256fe8557` | Fuse mov+add → leaq $300(%r12), %rax | -6.5 | 2026-06-02T09:01:23 |
| G0212 | I2 | `d274e53854da` | Replace cmpq $0,%rax with testq %rax,%rax, Fuse mov+add → leaq $140(%r12), %rax (+1) | -7.3 | 2026-06-02T09:02:33 |
| G0213 | I1 | `7e017b458e80` | Fuse mov+add → leaq $536(%r13), %rax | -3.8 | 2026-06-02T09:03:20 |
| G0214 | I2 | `b6a3127c6303` | Fuse mov+add → leaq $26344(%r12), %rax, Fuse mov+add → leaq $260(%r12), %rax | -20.3 | 2026-06-02T09:03:43 |
| G0215 | I1 | `5e56c4d844f0` | Replace cmpq $0,%rax with testq %rax,%rax, Fuse mov+add → leaq $16520(%r13), %rax | -1.8 | 2026-06-02T09:04:29 |
| G0216 | I1 | `49c77802b853` | Replace cmpq $0,%rax with testq %rax,%rax, Fuse mov+add → leaq $128(%r13), %rax (+1) | -13.1 | 2026-06-02T09:05:39 |
| G0217 | I0 | `05b525ab9db6` | Swap independent pair to reduce WAR pipeline stall, Fuse mov+add → leaq $18312(%r14), %rax (+1) | -2.8 | 2026-06-02T09:06:23 |
| G0218 | I1 | `a3dfb08118b7` | Replace cmpq $0,%rax with testq %rax,%rax | -3.7 | 2026-06-02T09:06:46 |
| G0219 | I2 | `cf90f0d78eb8` | Replace cmpq $0,%rax with testq %rax,%rax, Replace cmpq $0,%rax with testq %rax,%rax | -1.2 | 2026-06-02T09:07:06 |
| G0220 | I0 | `cb16b18b414a` | Replace cmpq $0,%rax with testq %rax,%rax | -3.3 | 2026-06-02T09:07:27 |
| G0221 | I1 | `072d97b6bda4` | Replace cmpq $0,%rax with testq %rax,%rax, Fuse mov+add → leaq $16520(%r13), %rax | -7.2 | 2026-06-02T09:07:50 |
| G0222 | I0 | `e75786ac3f6e` | Replace cmpq $0,%rax with testq %rax,%rax, Replace cmpq $0,%rax with testq %rax,%rax | -2.1 | 2026-06-03T09:43:31 |
| G0223 | I1 | `8d9d0592ba6b` | Replace cmpq $0,%rax with testq %rax,%rax | -2.6 | 2026-06-03T09:43:54 |
| G0224 | I0 | `1be1c08595de` | Sink load of -648(%rbp) to %r12 past independent instruction | -3.0 | 2026-06-03T09:44:38 |
| G0225 | I1 | `b92b1a153a79` | Sink load of -2272(%rbp) to %r13 past independent instruction | -1.2 | 2026-06-03T09:45:00 |
| G0226 | I2 | `a2a72c6588c9` | Fuse mov+add → leaq $24(%r14), %rax | -11.9 | 2026-06-03T09:45:23 |
| G0227 | I0 | `6ae328b985dd` | Fuse mov+add → leaq $20(%r12), %rax, Fuse mov+add → leaq $200(%r13), %rax | -19.9 | 2026-06-03T09:45:45 |
| G0228 | I1 | `9a27a8526367` | Fuse mov+add → leaq $24(%r14), %rax, Fuse mov+add → leaq $6849024(%r12), %rax (+1) | -17.2 | 2026-06-03T09:46:07 |
| G0229 | I2 | `3c01d715abf1` | Sink load of -39376(%rbp) to %r13 past independent instruction | -4.1 | 2026-06-03T09:46:29 |
| G0230 | I1 | `3027dada8ae7` | Sink load of -1792(%rbp) to %r13 past independent instruction, Sink load of -40(%rbp) to %r13 past independent instruction (+1) | -8.6 | 2026-06-03T09:47:13 |
| G0231 | I0 | `5b8e8e9faebb` | Fuse mov+add → leaq $192(%r14), %rax, Sink load of -432(%rbp) to %r13 past independent instruction (+1) | -16.5 | 2026-06-03T09:47:58 |
| G0232 | I1 | `930d637efb08` | Sink load of -640(%rbp) to %r13 past independent instruction, Fuse mov+add → leaq $18128(%r13), %rax (+1) | -9.0 | 2026-06-03T09:49:25 |
| G0233 | I2 | `94b7e94e7747` | Sink load of -48(%rbp) to %r15 past independent instruction, Fuse mov+add → leaq $24(%r12), %rax (+1) | -8.7 | 2026-06-03T09:49:47 |
| G0234 | I0 | `03e7c687c116` | Fuse mov+add → leaq $48(%r13), %rax, Fuse mov+add → leaq $12(%r14), %rax (+1) | -26.7 | 2026-06-03T09:50:09 |
| G0235 | I1 | `4c560e2abe74` | Sink load of -488(%rbp) to %r12 past independent instruction, Fuse mov+add → leaq $26328(%r12), %rax | -7.9 | 2026-06-03T09:50:31 |
| G0236 | I1 | `b54a57da11cf` | Sink load of -72(%rbp) to %r13 past independent instruction | -0.3 | 2026-06-03T09:51:36 |
| G0237 | I2 | `7fc4b6d44ebd` | Fuse mov+add → leaq $24(%r13), %rax, Swap independent pair to reduce WAR pipeline stall | -9.0 | 2026-06-03T09:51:57 |
| G0238 | I0 | `eb6a45b9f870` | Fuse mov+add → leaq $224(%r14), %rax | -8.1 | 2026-06-03T09:53:24 |
| G0239 | I1 | `dfe1b577001b` | Sink load of -920(%rbp) to %r14 past independent instruction, Fuse mov+add → leaq $140(%r12), %rax | -0.5 | 2026-06-03T09:53:46 |
| G0240 | I1 | `e077ad516b3f` | Sink load of -24(%rbp) to %r13 past independent instruction, Sink load of -32(%rbp) to %r14 past independent instruction (+1) | -19.3 | 2026-06-03T09:54:51 |
| G0241 | I2 | `a232062f69fd` | Reorder 3-instruction window to reduce WAR pipeline stall, Remove redundant load: %rax already has -344(%rbp) (+1) | -12.3 | 2026-06-03T09:55:15 |
| G0242 | I0 | `c16f7896e952` | Fuse mov+add → leaq $64(%r13), %rax, Sink load of -632(%rbp) to %r14 past independent instruction (+1) | -7.9 | 2026-06-03T09:55:37 |
| G0243 | I1 | `ec72a61abf25` | Fuse mov+add → leaq $368(%r13), %rax, Sink load of -816(%rbp) to %rbx past independent instruction | -8.5 | 2026-06-03T09:55:59 |
| G0244 | I2 | `7bab3d5f9006` | Sink load of -48(%rbp) to %r12 past independent instruction, Fuse mov+add → leaq $32(%r13), %rax | -6.3 | 2026-06-03T09:56:21 |
| G0245 | I0 | `4806a68b7092` | Fuse mov+add → leaq $136(%r12), %rax, Sink load of -39376(%rbp) to %r13 past independent instruction | -9.0 | 2026-06-03T09:56:43 |
| G0246 | I1 | `57e7e7ca827e` | Sink load of -584(%rbp) to %r14 past independent instruction, Fuse mov+add → leaq $140(%r12), %rax | -9.2 | 2026-06-03T09:57:05 |
| G0247 | I1 | `e39307e0917d` | Sink load of -520(%rbp) to %r14 past independent instruction, Sink load of -584(%rbp) to %r12 past independent instruction (+1) | -5.7 | 2026-06-03T09:58:11 |
| G0248 | I2 | `ffc40cccc19e` | Sink load of -144(%rbp) to %r14 past independent instruction, Sink load of -40(%rbp) to %r13 past independent instruction (+1) | -1.1 | 2026-06-03T09:58:33 |
| G0249 | I2 | `9b51f22f3692` | Fuse mov+add → leaq $300(%r12), %rax, Fuse mov+add → leaq $300(%r12), %rax (+1) | -10.7 | 2026-06-03T10:00:43 |
| G0250 | I0 | `f3a201cdb919` | Fuse mov+add → leaq $132(%r12), %rax | -8.1 | 2026-06-03T10:01:05 |
| G0251 | I1 | `09885ed4e33f` | Fuse mov+add → leaq $4(%r12), %rax, Sink load of -39376(%rbp) to %r13 past independent instruction (+1) | -12.6 | 2026-06-03T10:01:29 |
| G0252 | I1 | `06278056378b` | Swap independent pair to reduce WAR pipeline stall | -5.0 | 2026-06-04T00:30:39 |
| G0253 | I2 | `90839c7084b2` | Sink load of -5776(%rbp) to %rbx past independent instruction, Fuse mov+add → leaq $308(%r13), %rax | -38.5 | 2026-06-04T00:31:02 |
| G0254 | I0 | `705dc751e170` | Sink load of -648(%rbp) to %r12 past independent instruction | -9.4 | 2026-06-04T00:31:26 |
| G0255 | I0 | `d4e9d84b6f82` | Fuse mov+add → leaq $24(%r12), %rax, Fuse mov+add → leaq $128(%r12), %rax | -7.2 | 2026-06-04T00:32:38 |
| G0256 | I1 | `9cfe4d9126c8` | Fuse mov+add → leaq $24(%r14), %rax, Fuse mov+add → leaq $6849024(%r12), %rax (+1) | -22.6 | 2026-06-04T00:33:02 |
| G0257 | I0 | `dfcc83a5eb6a` | Sink load of -696(%rbp) to %r14 past independent instruction | -9.8 | 2026-06-04T00:33:50 |
| G0258 | I1 | `56094ddf11a5` | Sink load of -96(%rbp) to %rbx past independent instruction, Fuse mov+add → leaq $56(%rbx), %rax | -9.3 | 2026-06-04T00:35:26 |
| G0259 | I1 | `f57d2aa64095` | Fuse mov+add → leaq $104(%r12), %rax, Sink load of -496(%rbp) to %rbx past independent instruction (+1) | -3.4 | 2026-06-04T00:36:43 |
| G0260 | I0 | `0cea4a5b9269` | Sink load of -34592(%rbp) to %r13 past independent instruction, Fuse mov+add → leaq $24(%r14), %rax (+1) | -4.2 | 2026-06-04T00:43:57 |
| G0261 | I0 | `0ec21c6a9c6c` | Fuse mov+add → leaq $32(%r15), %rax | -5.6 | 2026-06-04T00:46:37 |
| G0262 | I0 | `4d87a65cdf68` | Fuse mov+add → leaq $24(%r13), %rax, Sink load of -632(%rbp) to %r14 past independent instruction | -9.3 | 2026-06-04T01:14:27 |
| G0263 | I2 | `90839c7084b2` | Sink load of -5776(%rbp) to %rbx past independent instruction, Fuse mov+add → leaq $308(%r13), %rax | -5.9 | 2026-06-04T01:15:14 |
| G0264 | I1 | `21fe9425278d` | Fuse mov+add → leaq $200(%r13), %rax, Fuse mov+add → leaq $20(%r12), %rax (+1) | -4.3 | 2026-06-04T01:17:13 |
| G0265 | I0 | `91a5e4d98857` | Fuse mov+add → leaq $192(%r13), %rax | -1.4 | 2026-06-04T01:18:03 |
| G0266 | I1 | `f4f9411cc2ab` | Sink load of -648(%rbp) to %r14 past independent instruction, Sink load of -440(%rbp) to %r14 past independent instruction (+1) | -9.3 | 2026-06-04T01:18:27 |
| G0267 | I2 | `942757ae8cbe` | Sink load of -936(%rbp) to %r14 past independent instruction, Fuse mov+add → leaq $25300(%r12), %rax (+1) | -2.0 | 2026-06-04T01:18:52 |
| G0268 | I1 | `c1d57d026684` | Sink load of -64(%rbp) to %r12 past independent instruction, Sink load of -39384(%rbp) to %r14 past independent instruction | -0.0 | 2026-06-04T01:19:51 |
| G0269 | I2 | `7d598b66057e` | Fuse mov+add → leaq $264(%r12), %rax | -13.6 | 2026-06-04T01:20:16 |
| G0270 | I0 | `46eadb81e4d8` | Sink load of -48(%rbp) to %r12 past independent instruction, Fuse mov+add → leaq $192(%r14), %rax | -12.0 | 2026-06-04T01:20:41 |
| G0271 | I1 | `b51a7ad873a5` | Fuse mov+add → leaq $18308(%r12), %rax, Fuse mov+add → leaq $156(%r13), %rax (+1) | -13.5 | 2026-06-04T01:21:10 |
| G0272 | I2 | `a1aa7f6b9457` | Sink load of -40(%rbp) to %r12 past independent instruction, Sink load of -39368(%rbp) to %r12 past independent instruction (+1) | -8.6 | 2026-06-04T01:21:34 |
| G0273 | I0 | `cc715bad53c9` | Sink load of -24(%rbp) to %r13 past independent instruction, Sink load of -584(%rbp) to %r14 past independent instruction (+1) | -5.3 | 2026-06-04T01:21:58 |
| G0274 | I1 | `5e3084bc51f2` | Sink load of -32(%rbp) to %r12 past independent instruction, Fuse mov+add → leaq $264(%r12), %rax | -12.1 | 2026-06-04T01:22:24 |
| G0275 | I2 | `288ff4e03256` | Sink load of -32(%rbp) to %r12 past independent instruction, Fuse mov+add → leaq $6849024(%r12), %rax | -8.0 | 2026-06-04T01:24:03 |
| G0276 | I0 | `e597f91702a8` | Fuse mov+add → leaq $16(%r13), %rax, Sink load of -48(%rbp) to %r12 past independent instruction (+1) | -6.1 | 2026-06-04T01:24:29 |
| G0277 | I0 | `a089c257e806` | Sink load of -48(%rbp) to %r13 past independent instruction, Fuse mov+add → leaq $336(%r13), %rax (+1) | -17.1 | 2026-06-04T01:28:34 |
| G0278 | I2 | `f6225f6cc5dc` | Fuse mov+add → leaq $32(%r13), %rax, Sink load of -39368(%rbp) to %r12 past independent instruction (+1) | -10.4 | 2026-06-04T01:35:25 |
| G0279 | I1 | `e813cb4a1497` | Fuse mov+add → leaq $128(%r12), %rax, Sink load of -656(%rbp) to %r13 past independent instruction (+1) | -8.5 | 2026-06-04T01:36:15 |
| G0280 | I0 | `8fa7819caab8` | Sink load of -520(%rbp) to %r14 past independent instruction, Sink load of -704(%rbp) to %r13 past independent instruction | -4.9 | 2026-06-04T20:27:19 |
| G0281 | I1 | `9d4ce27d1975` | Fuse mov+add → leaq $68(%r13), %rax | -8.4 | 2026-06-04T20:27:41 |
| G0282 | I2 | `a3526b996466` | Sink load of -1064(%rbp) to %r12 past independent instruction, Fuse mov+add → leaq $56(%r14), %rax | -12.6 | 2026-06-04T20:28:02 |
| G0283 | I1 | `1c2176bf1462` | Fuse mov+add → leaq $16520(%r13), %rax, Fuse mov+add → leaq $136(%r12), %rax (+1) | -11.8 | 2026-06-04T20:29:57 |
| G0284 | I1 | `66acf6bfadc2` | Fuse mov+add → leaq $136(%r12), %rax, Fuse mov+add → leaq $2056(%r12), %rax (+1) | -11.6 | 2026-06-04T20:31:09 |
| G0285 | I2 | `304e4d258869` | Fuse mov+add → leaq $270992144(%r14), %rax, Fuse mov+add → leaq $24(%r15), %rax (+1) | -4.2 | 2026-06-04T20:31:34 |
| G0286 | I2 | `56338924d2e2` | Sink load of -5768(%rbp) to %r15 past independent instruction | -14.8 | 2026-06-04T20:32:48 |
| G0287 | I1 | `9ae9893fb823` | Sink load of -39368(%rbp) to %r12 past independent instruction, Fuse mov+add → leaq $24(%r12), %rax (+1) | -25.1 | 2026-06-04T20:33:37 |
| G0288 | I2 | `450713e8fb70` | Fuse mov+add → leaq $12(%r12), %rax | -7.2 | 2026-06-04T20:35:06 |
| G0289 | I2 | `5e27f0dfaa84` | Sink load of -56(%rbp) to %r12 past independent instruction, Sink load of -624(%rbp) to %rbx past independent instruction | -1.4 | 2026-06-04T20:36:19 |
| G0290 | I0 | `cde1594a2b3e` | Sink load of -504(%rbp) to %r14 past independent instruction, Fuse mov+add → leaq $12(%r14), %rax (+1) | -1.4 | 2026-06-04T20:36:41 |
| G0291 | I0 | `8120275758f2` | Fuse mov+add → leaq $140(%r12), %rax | -16.6 | 2026-06-04T20:39:00 |
| G0292 | I2 | `dbc4008b1d59` | Sink load of -1048(%rbp) to %r14 past independent instruction, Fuse mov+add → leaq $52(%r13), %rax (+1) | -6.3 | 2026-06-04T20:39:48 |
| G0293 | I0 | `77825deca585` | Fuse mov+add → leaq $128(%r13), %rax, Fuse mov+add → leaq $140(%r12), %rax | -8.0 | 2026-06-04T20:41:20 |
| G0294 | I2 | `3855ced4af0c` | Sink load of -72(%rbp) to %r15 past independent instruction, Fuse mov+add → leaq $40(%r13), %rax | -3.6 | 2026-06-04T20:42:07 |
| G0295 | I2 | `2f5e6d39de69` | Sink load of -624(%rbp) to %rbx past independent instruction, Fuse mov+add → leaq $192(%r14), %rax (+1) | -10.4 | 2026-06-04T20:43:20 |
| G0296 | I2 | `ed7b7af95f3b` | Fuse mov+add → leaq $388(%r13), %rax | -10.6 | 2026-06-04T20:44:33 |
| G0297 | I2 | `e36f541495e9` | Sink load of -704(%rbp) to %r14 past independent instruction, Fuse mov+add → leaq $24(%rbx), %rax (+1) | -13.1 | 2026-06-04T20:45:46 |
| G0298 | I0 | `fc61187b8eb6` | Sink load of -39376(%rbp) to %r13 past independent instruction | -10.0 | 2026-06-04T20:46:11 |
| G0299 | I2 | `7952cfad76fe` | Sink load of -39360(%rbp) to %rbx past independent instruction | -4.5 | 2026-06-04T20:50:50 |
| G0300 | I0 | `5bf22fde7224` | Sink load of -48(%rbp) to %r12 past independent instruction, Sink load of -2056(%rbp) to %r14 past independent instruction (+1) | -2.2 | 2026-06-04T20:51:14 |
| G0301 | I0 | `6d7d57ad76e3` | Sink load of -496(%rbp) to %r13 past independent instruction, Sink load of -1048(%rbp) to %r14 past independent instruction | -2.6 | 2026-06-04T20:52:19 |
| G0302 | I0 | `2e83cace06a7` | Sink load of -912(%rbp) to %rbx past independent instruction, Fuse mov+add → leaq $16(%r13), %rax | -8.6 | 2026-06-04T20:53:24 |
| G0303 | I1 | `091782708ac4` | Fuse mov+add → leaq $156(%r13), %rax | -7.8 | 2026-06-04T20:53:45 |
| G0304 | I2 | `0da4749a4b8f` | Sink load of -936(%rbp) to %rbx past independent instruction | -0.0 | 2026-06-04T20:54:06 |
| G0305 | I0 | `2962e9c1d7a9` | Fuse mov+add → leaq $336(%r12), %rax | -6.9 | 2026-06-04T20:54:27 |
| G0306 | I1 | `7c07ecf84473` | Fuse mov+add → leaq $24(%r15), %rax | -1.0 | 2026-06-04T20:54:48 |
| G0307 | I0 | `d34c525abcaa` | Sink load of -424(%rbp) to %rax past independent instruction, Sink load of -48(%rbp) to %rbx past independent instruction (+1) | -0.8 | 2026-06-04T20:55:31 |
| G0308 | I1 | `6871e457044a` | Sink load of -32(%rbp) to %r13 past independent instruction, Fuse mov+add → leaq $140(%r12), %rax | -17.4 | 2026-06-04T20:55:52 |
| G0309 | I2 | `9d6ea63737aa` | Fuse mov+add → leaq $16(%r15), %rax, Sink load of -56(%rbp) to %r13 past independent instruction (+1) | -6.9 | 2026-06-04T20:56:13 |
| G0310 | I0 | `7521ad9ad9b2` | Fuse mov+add → leaq $192(%r14), %rax, Fuse mov+add → leaq $12(%r14), %rax | -17.7 | 2026-06-04T20:56:34 |
| G0311 | I2 | `268086a9aeff` | Sink load of -696(%rbp) to %r14 past independent instruction | -0.8 | 2026-06-04T20:57:17 |
| G0312 | I0 | `78563466600b` | Sink load of -39384(%rbp) to %r14 past independent instruction, Fuse mov+add → leaq $16520(%rbx), %rax | -1.4 | 2026-06-04T20:57:39 |
| G0313 | I2 | `95ccd56d96a3` | Sink load of -40(%rbp) to %r15 past independent instruction, Fuse mov+add → leaq $20(%r12), %rax (+1) | -8.4 | 2026-06-04T20:58:22 |
| G0314 | I0 | `875efd07360d` | Fuse mov+add → leaq $192(%r14), %rax | -13.9 | 2026-06-04T20:58:42 |
| G0315 | I1 | `7d261040e5aa` | Fuse mov+add → leaq $296(%r13), %rax | -8.3 | 2026-06-04T20:59:04 |
| G0316 | I2 | `8ebcd8b6bbc3` | Reorder 3-instruction window to reduce WAR pipeline stall, Fuse mov+add → leaq $4(%r12), %rax | -6.2 | 2026-06-04T20:59:25 |
| G0317 | I1 | `65c72b59f369` | Fuse mov+add → leaq $24(%r12), %rax, Sink load of -640(%rbp) to %r13 past independent instruction | -8.7 | 2026-06-04T21:00:08 |
| G0318 | I2 | `f058c71d9ff6` | Sink load of -56(%rbp) to %r13 past independent instruction, Fuse mov+add → leaq $8(%r12), %rax (+1) | -0.4 | 2026-06-04T21:00:29 |
| G0319 | I1 | `114a1f23a327` | Sink load of -736(%rbp) to %rbx past independent instruction, Sink load of -632(%rbp) to %r12 past independent instruction (+1) | -8.7 | 2026-06-04T21:01:11 |
| G0320 | I2 | `5678b496f7df` | Sink load of -504(%rbp) to %r14 past independent instruction, Sink load of -96(%rbp) to %r13 past independent instruction | -10.1 | 2026-06-04T21:01:32 |
| G0321 | I0 | `ae9f0dc02c88` | Sink load of -32(%rbp) to %r14 past independent instruction, Fuse mov+add → leaq $192(%r15), %rax (+1) | -3.0 | 2026-06-04T21:01:56 |
| G0322 | I1 | `32e116784fe5` | Fuse mov+add → leaq $6849024(%r12), %rax | -7.3 | 2026-06-04T21:02:17 |
| G0323 | I2 | `cc1b3fbc23c8` | Sink load of -360(%rbp) to %r14 past independent instruction, Sink load of -520(%rbp) to %r14 past independent instruction | -0.9 | 2026-06-04T21:02:38 |
| G0324 | I2 | `ff38245cd509` | Fuse mov+add → leaq $152(%r14), %rax, Sink load of -72(%rbp) to %r13 past independent instruction | -8.2 | 2026-06-04T21:03:43 |
| G0325 | I2 | `cff6b17be5f0` | Sink load of -296(%rbp) to %r14 past independent instruction, Remove dead movq to %rax (overwritten at +1) (+1) | -9.9 | 2026-06-04T21:04:46 |
| G0326 | I0 | `02b835c2fc6c` | Sink load of -632(%rbp) to %r12 past independent instruction, Fuse mov+add → leaq $16528(%r13), %rax | -11.5 | 2026-06-04T21:05:07 |
| G0327 | I1 | `5b8fffff8e2f` | Sink load of -480(%rbp) to %rbx past independent instruction, Fuse mov+add → leaq $336(%r13), %rax | -5.5 | 2026-06-04T21:05:29 |
| G0328 | I0 | `db70be989f91` | Fuse mov+add → leaq $560(%r13), %rax, Sink load of -496(%rbp) to %r13 past independent instruction | -2.6 | 2026-06-04T21:06:12 |
| G0329 | I1 | `c730498e0600` | Swap independent pair to reduce WAR pipeline stall, Fuse mov+add → leaq $64(%r12), %rax | -9.1 | 2026-06-04T21:06:35 |
| G0330 | I0 | `92974f110d5d` | Sink load of -34616(%rbp) to %rbx past independent instruction | -2.6 | 2026-06-04T21:07:18 |

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
- `QAlgo-Dream-G132` → [`QAlgo-Dream-G132.json`](journal/QAlgo-Dream-G132.json)
- `QAlgo-Dream-G133` → [`QAlgo-Dream-G133.json`](journal/QAlgo-Dream-G133.json)
- `QAlgo-Dream-G134` → [`QAlgo-Dream-G134.json`](journal/QAlgo-Dream-G134.json)
- `QAlgo-Dream-G135` → [`QAlgo-Dream-G135.json`](journal/QAlgo-Dream-G135.json)
- `QAlgo-Dream-G136` → [`QAlgo-Dream-G136.json`](journal/QAlgo-Dream-G136.json)
- `QAlgo-Dream-G137` → [`QAlgo-Dream-G137.json`](journal/QAlgo-Dream-G137.json)
- `QAlgo-Dream-G138` → [`QAlgo-Dream-G138.json`](journal/QAlgo-Dream-G138.json)
- `QAlgo-Dream-G139` → [`QAlgo-Dream-G139.json`](journal/QAlgo-Dream-G139.json)
- `QAlgo-Dream-G140` → [`QAlgo-Dream-G140.json`](journal/QAlgo-Dream-G140.json)
- `QAlgo-Dream-G141` → [`QAlgo-Dream-G141.json`](journal/QAlgo-Dream-G141.json)
- `QAlgo-Dream-G142` → [`QAlgo-Dream-G142.json`](journal/QAlgo-Dream-G142.json)
- `QAlgo-Dream-G143` → [`QAlgo-Dream-G143.json`](journal/QAlgo-Dream-G143.json)
- `QAlgo-Dream-G144` → [`QAlgo-Dream-G144.json`](journal/QAlgo-Dream-G144.json)
- `QAlgo-Dream-G145` → [`QAlgo-Dream-G145.json`](journal/QAlgo-Dream-G145.json)
- `QAlgo-Dream-G146` → [`QAlgo-Dream-G146.json`](journal/QAlgo-Dream-G146.json)
- `QAlgo-Dream-G147` → [`QAlgo-Dream-G147.json`](journal/QAlgo-Dream-G147.json)
- `QAlgo-Dream-G148` → [`QAlgo-Dream-G148.json`](journal/QAlgo-Dream-G148.json)
- `QAlgo-Dream-G149` → [`QAlgo-Dream-G149.json`](journal/QAlgo-Dream-G149.json)
- `QAlgo-Dream-G150` → [`QAlgo-Dream-G150.json`](journal/QAlgo-Dream-G150.json)
- `QAlgo-Dream-G151` → [`QAlgo-Dream-G151.json`](journal/QAlgo-Dream-G151.json)
- `QAlgo-Dream-G152` → [`QAlgo-Dream-G152.json`](journal/QAlgo-Dream-G152.json)
- `QAlgo-Dream-G153` → [`QAlgo-Dream-G153.json`](journal/QAlgo-Dream-G153.json)
- `QAlgo-Dream-G154` → [`QAlgo-Dream-G154.json`](journal/QAlgo-Dream-G154.json)
- `QAlgo-Dream-G155` → [`QAlgo-Dream-G155.json`](journal/QAlgo-Dream-G155.json)
- `QAlgo-Dream-G156` → [`QAlgo-Dream-G156.json`](journal/QAlgo-Dream-G156.json)
- `QAlgo-Dream-G157` → [`QAlgo-Dream-G157.json`](journal/QAlgo-Dream-G157.json)
- `QAlgo-Dream-G158` → [`QAlgo-Dream-G158.json`](journal/QAlgo-Dream-G158.json)
- `QAlgo-Dream-G159` → [`QAlgo-Dream-G159.json`](journal/QAlgo-Dream-G159.json)
- `QAlgo-Dream-G160` → [`QAlgo-Dream-G160.json`](journal/QAlgo-Dream-G160.json)
- `QAlgo-Dream-G161` → [`QAlgo-Dream-G161.json`](journal/QAlgo-Dream-G161.json)
- `QAlgo-Dream-G162` → [`QAlgo-Dream-G162.json`](journal/QAlgo-Dream-G162.json)
- `QAlgo-Dream-G163` → [`QAlgo-Dream-G163.json`](journal/QAlgo-Dream-G163.json)
- `QAlgo-Dream-G164` → [`QAlgo-Dream-G164.json`](journal/QAlgo-Dream-G164.json)
- `QAlgo-Dream-G165` → [`QAlgo-Dream-G165.json`](journal/QAlgo-Dream-G165.json)
- `QAlgo-Dream-G166` → [`QAlgo-Dream-G166.json`](journal/QAlgo-Dream-G166.json)
- `QAlgo-Dream-G167` → [`QAlgo-Dream-G167.json`](journal/QAlgo-Dream-G167.json)
- `QAlgo-Dream-G168` → [`QAlgo-Dream-G168.json`](journal/QAlgo-Dream-G168.json)
- `QAlgo-Dream-G169` → [`QAlgo-Dream-G169.json`](journal/QAlgo-Dream-G169.json)
- `QAlgo-Dream-G170` → [`QAlgo-Dream-G170.json`](journal/QAlgo-Dream-G170.json)
- `QAlgo-Dream-G171` → [`QAlgo-Dream-G171.json`](journal/QAlgo-Dream-G171.json)
- `QAlgo-Dream-G172` → [`QAlgo-Dream-G172.json`](journal/QAlgo-Dream-G172.json)
- `QAlgo-Dream-G173` → [`QAlgo-Dream-G173.json`](journal/QAlgo-Dream-G173.json)
- `QAlgo-Dream-G174` → [`QAlgo-Dream-G174.json`](journal/QAlgo-Dream-G174.json)
- `QAlgo-Dream-G175` → [`QAlgo-Dream-G175.json`](journal/QAlgo-Dream-G175.json)
- `QAlgo-Dream-G176` → [`QAlgo-Dream-G176.json`](journal/QAlgo-Dream-G176.json)
- `QAlgo-Dream-G177` → [`QAlgo-Dream-G177.json`](journal/QAlgo-Dream-G177.json)
- `QAlgo-Dream-G178` → [`QAlgo-Dream-G178.json`](journal/QAlgo-Dream-G178.json)
- `QAlgo-Dream-G179` → [`QAlgo-Dream-G179.json`](journal/QAlgo-Dream-G179.json)
- `QAlgo-Dream-G180` → [`QAlgo-Dream-G180.json`](journal/QAlgo-Dream-G180.json)
- `QAlgo-Dream-G181` → [`QAlgo-Dream-G181.json`](journal/QAlgo-Dream-G181.json)
- `QAlgo-Dream-G182` → [`QAlgo-Dream-G182.json`](journal/QAlgo-Dream-G182.json)
- `QAlgo-Dream-G183` → [`QAlgo-Dream-G183.json`](journal/QAlgo-Dream-G183.json)
- `QAlgo-Dream-G184` → [`QAlgo-Dream-G184.json`](journal/QAlgo-Dream-G184.json)
- `QAlgo-Dream-G185` → [`QAlgo-Dream-G185.json`](journal/QAlgo-Dream-G185.json)
- `QAlgo-Dream-G186` → [`QAlgo-Dream-G186.json`](journal/QAlgo-Dream-G186.json)
- `QAlgo-Dream-G187` → [`QAlgo-Dream-G187.json`](journal/QAlgo-Dream-G187.json)
- `QAlgo-Dream-G188` → [`QAlgo-Dream-G188.json`](journal/QAlgo-Dream-G188.json)
- `QAlgo-Dream-G189` → [`QAlgo-Dream-G189.json`](journal/QAlgo-Dream-G189.json)
- `QAlgo-Dream-G190` → [`QAlgo-Dream-G190.json`](journal/QAlgo-Dream-G190.json)
- `QAlgo-Dream-G191` → [`QAlgo-Dream-G191.json`](journal/QAlgo-Dream-G191.json)
- `QAlgo-Dream-G192` → [`QAlgo-Dream-G192.json`](journal/QAlgo-Dream-G192.json)
- `QAlgo-Dream-G193` → [`QAlgo-Dream-G193.json`](journal/QAlgo-Dream-G193.json)
- `QAlgo-Dream-G194` → [`QAlgo-Dream-G194.json`](journal/QAlgo-Dream-G194.json)
- `QAlgo-Dream-G195` → [`QAlgo-Dream-G195.json`](journal/QAlgo-Dream-G195.json)
- `QAlgo-Dream-G196` → [`QAlgo-Dream-G196.json`](journal/QAlgo-Dream-G196.json)
- `QAlgo-Dream-G197` → [`QAlgo-Dream-G197.json`](journal/QAlgo-Dream-G197.json)
- `QAlgo-Dream-G198` → [`QAlgo-Dream-G198.json`](journal/QAlgo-Dream-G198.json)
- `QAlgo-Dream-G199` → [`QAlgo-Dream-G199.json`](journal/QAlgo-Dream-G199.json)
- `QAlgo-Dream-G200` → [`QAlgo-Dream-G200.json`](journal/QAlgo-Dream-G200.json)
- `QAlgo-Dream-G201` → [`QAlgo-Dream-G201.json`](journal/QAlgo-Dream-G201.json)
- `QAlgo-Dream-G202` → [`QAlgo-Dream-G202.json`](journal/QAlgo-Dream-G202.json)
- `QAlgo-Dream-G203` → [`QAlgo-Dream-G203.json`](journal/QAlgo-Dream-G203.json)
- `QAlgo-Dream-G204` → [`QAlgo-Dream-G204.json`](journal/QAlgo-Dream-G204.json)
- `QAlgo-Dream-G205` → [`QAlgo-Dream-G205.json`](journal/QAlgo-Dream-G205.json)
- `QAlgo-Dream-G206` → [`QAlgo-Dream-G206.json`](journal/QAlgo-Dream-G206.json)
- `QAlgo-Dream-G207` → [`QAlgo-Dream-G207.json`](journal/QAlgo-Dream-G207.json)
- `QAlgo-Dream-G208` → [`QAlgo-Dream-G208.json`](journal/QAlgo-Dream-G208.json)
- `QAlgo-Dream-G209` → [`QAlgo-Dream-G209.json`](journal/QAlgo-Dream-G209.json)
- `QAlgo-Dream-G210` → [`QAlgo-Dream-G210.json`](journal/QAlgo-Dream-G210.json)
- `QAlgo-Dream-G211` → [`QAlgo-Dream-G211.json`](journal/QAlgo-Dream-G211.json)
- `QAlgo-Dream-G212` → [`QAlgo-Dream-G212.json`](journal/QAlgo-Dream-G212.json)
- `QAlgo-Dream-G213` → [`QAlgo-Dream-G213.json`](journal/QAlgo-Dream-G213.json)
- `QAlgo-Dream-G214` → [`QAlgo-Dream-G214.json`](journal/QAlgo-Dream-G214.json)
- `QAlgo-Dream-G215` → [`QAlgo-Dream-G215.json`](journal/QAlgo-Dream-G215.json)
- `QAlgo-Dream-G216` → [`QAlgo-Dream-G216.json`](journal/QAlgo-Dream-G216.json)
- `QAlgo-Dream-G217` → [`QAlgo-Dream-G217.json`](journal/QAlgo-Dream-G217.json)
- `QAlgo-Dream-G218` → [`QAlgo-Dream-G218.json`](journal/QAlgo-Dream-G218.json)
- `QAlgo-Dream-G219` → [`QAlgo-Dream-G219.json`](journal/QAlgo-Dream-G219.json)
- `QAlgo-Dream-G220` → [`QAlgo-Dream-G220.json`](journal/QAlgo-Dream-G220.json)
- `QAlgo-Dream-G221` → [`QAlgo-Dream-G221.json`](journal/QAlgo-Dream-G221.json)
- `QAlgo-Dream-G222` → [`QAlgo-Dream-G222.json`](journal/QAlgo-Dream-G222.json)
- `QAlgo-Dream-G223` → [`QAlgo-Dream-G223.json`](journal/QAlgo-Dream-G223.json)
- `QAlgo-Dream-G224` → [`QAlgo-Dream-G224.json`](journal/QAlgo-Dream-G224.json)
- `QAlgo-Dream-G225` → [`QAlgo-Dream-G225.json`](journal/QAlgo-Dream-G225.json)
- `QAlgo-Dream-G226` → [`QAlgo-Dream-G226.json`](journal/QAlgo-Dream-G226.json)
- `QAlgo-Dream-G227` → [`QAlgo-Dream-G227.json`](journal/QAlgo-Dream-G227.json)
- `QAlgo-Dream-G228` → [`QAlgo-Dream-G228.json`](journal/QAlgo-Dream-G228.json)
- `QAlgo-Dream-G229` → [`QAlgo-Dream-G229.json`](journal/QAlgo-Dream-G229.json)
- `QAlgo-Dream-G230` → [`QAlgo-Dream-G230.json`](journal/QAlgo-Dream-G230.json)
- `QAlgo-Dream-G231` → [`QAlgo-Dream-G231.json`](journal/QAlgo-Dream-G231.json)
- `QAlgo-Dream-G232` → [`QAlgo-Dream-G232.json`](journal/QAlgo-Dream-G232.json)
- `QAlgo-Dream-G233` → [`QAlgo-Dream-G233.json`](journal/QAlgo-Dream-G233.json)
- `QAlgo-Dream-G234` → [`QAlgo-Dream-G234.json`](journal/QAlgo-Dream-G234.json)
- `QAlgo-Dream-G235` → [`QAlgo-Dream-G235.json`](journal/QAlgo-Dream-G235.json)
- `QAlgo-Dream-G236` → [`QAlgo-Dream-G236.json`](journal/QAlgo-Dream-G236.json)
- `QAlgo-Dream-G237` → [`QAlgo-Dream-G237.json`](journal/QAlgo-Dream-G237.json)
- `QAlgo-Dream-G238` → [`QAlgo-Dream-G238.json`](journal/QAlgo-Dream-G238.json)
- `QAlgo-Dream-G239` → [`QAlgo-Dream-G239.json`](journal/QAlgo-Dream-G239.json)
- `QAlgo-Dream-G240` → [`QAlgo-Dream-G240.json`](journal/QAlgo-Dream-G240.json)
- `QAlgo-Dream-G241` → [`QAlgo-Dream-G241.json`](journal/QAlgo-Dream-G241.json)
- `QAlgo-Dream-G242` → [`QAlgo-Dream-G242.json`](journal/QAlgo-Dream-G242.json)
- `QAlgo-Dream-G243` → [`QAlgo-Dream-G243.json`](journal/QAlgo-Dream-G243.json)
- `QAlgo-Dream-G244` → [`QAlgo-Dream-G244.json`](journal/QAlgo-Dream-G244.json)
- `QAlgo-Dream-G245` → [`QAlgo-Dream-G245.json`](journal/QAlgo-Dream-G245.json)
- `QAlgo-Dream-G246` → [`QAlgo-Dream-G246.json`](journal/QAlgo-Dream-G246.json)
- `QAlgo-Dream-G247` → [`QAlgo-Dream-G247.json`](journal/QAlgo-Dream-G247.json)
- `QAlgo-Dream-G248` → [`QAlgo-Dream-G248.json`](journal/QAlgo-Dream-G248.json)
- `QAlgo-Dream-G249` → [`QAlgo-Dream-G249.json`](journal/QAlgo-Dream-G249.json)
- `QAlgo-Dream-G250` → [`QAlgo-Dream-G250.json`](journal/QAlgo-Dream-G250.json)
- `QAlgo-Dream-G251` → [`QAlgo-Dream-G251.json`](journal/QAlgo-Dream-G251.json)
- `QAlgo-Dream-G252` → [`QAlgo-Dream-G252.json`](journal/QAlgo-Dream-G252.json)
- `QAlgo-Dream-G253` → [`QAlgo-Dream-G253.json`](journal/QAlgo-Dream-G253.json)
- `QAlgo-Dream-G254` → [`QAlgo-Dream-G254.json`](journal/QAlgo-Dream-G254.json)
- `QAlgo-Dream-G255` → [`QAlgo-Dream-G255.json`](journal/QAlgo-Dream-G255.json)
- `QAlgo-Dream-G256` → [`QAlgo-Dream-G256.json`](journal/QAlgo-Dream-G256.json)
- `QAlgo-Dream-G257` → [`QAlgo-Dream-G257.json`](journal/QAlgo-Dream-G257.json)
- `QAlgo-Dream-G258` → [`QAlgo-Dream-G258.json`](journal/QAlgo-Dream-G258.json)
- `QAlgo-Dream-G259` → [`QAlgo-Dream-G259.json`](journal/QAlgo-Dream-G259.json)
- `QAlgo-Dream-G260` → [`QAlgo-Dream-G260.json`](journal/QAlgo-Dream-G260.json)
- `QAlgo-Dream-G261` → [`QAlgo-Dream-G261.json`](journal/QAlgo-Dream-G261.json)
- `QAlgo-Dream-G262` → [`QAlgo-Dream-G262.json`](journal/QAlgo-Dream-G262.json)
- `QAlgo-Dream-G263` → [`QAlgo-Dream-G263.json`](journal/QAlgo-Dream-G263.json)
- `QAlgo-Dream-G264` → [`QAlgo-Dream-G264.json`](journal/QAlgo-Dream-G264.json)
- `QAlgo-Dream-G265` → [`QAlgo-Dream-G265.json`](journal/QAlgo-Dream-G265.json)
- `QAlgo-Dream-G266` → [`QAlgo-Dream-G266.json`](journal/QAlgo-Dream-G266.json)
- `QAlgo-Dream-G267` → [`QAlgo-Dream-G267.json`](journal/QAlgo-Dream-G267.json)
- `QAlgo-Dream-G268` → [`QAlgo-Dream-G268.json`](journal/QAlgo-Dream-G268.json)
- `QAlgo-Dream-G269` → [`QAlgo-Dream-G269.json`](journal/QAlgo-Dream-G269.json)
- `QAlgo-Dream-G270` → [`QAlgo-Dream-G270.json`](journal/QAlgo-Dream-G270.json)
- `QAlgo-Dream-G271` → [`QAlgo-Dream-G271.json`](journal/QAlgo-Dream-G271.json)
- `QAlgo-Dream-G272` → [`QAlgo-Dream-G272.json`](journal/QAlgo-Dream-G272.json)
- `QAlgo-Dream-G273` → [`QAlgo-Dream-G273.json`](journal/QAlgo-Dream-G273.json)
- `QAlgo-Dream-G274` → [`QAlgo-Dream-G274.json`](journal/QAlgo-Dream-G274.json)
- `QAlgo-Dream-G275` → [`QAlgo-Dream-G275.json`](journal/QAlgo-Dream-G275.json)
- `QAlgo-Dream-G276` → [`QAlgo-Dream-G276.json`](journal/QAlgo-Dream-G276.json)
- `QAlgo-Dream-G277` → [`QAlgo-Dream-G277.json`](journal/QAlgo-Dream-G277.json)
- `QAlgo-Dream-G278` → [`QAlgo-Dream-G278.json`](journal/QAlgo-Dream-G278.json)
- `QAlgo-Dream-G279` → [`QAlgo-Dream-G279.json`](journal/QAlgo-Dream-G279.json)
- `QAlgo-Dream-G280` → [`QAlgo-Dream-G280.json`](journal/QAlgo-Dream-G280.json)
- `QAlgo-Dream-G281` → [`QAlgo-Dream-G281.json`](journal/QAlgo-Dream-G281.json)
- `QAlgo-Dream-G282` → [`QAlgo-Dream-G282.json`](journal/QAlgo-Dream-G282.json)
- `QAlgo-Dream-G283` → [`QAlgo-Dream-G283.json`](journal/QAlgo-Dream-G283.json)
- `QAlgo-Dream-G284` → [`QAlgo-Dream-G284.json`](journal/QAlgo-Dream-G284.json)
- `QAlgo-Dream-G285` → [`QAlgo-Dream-G285.json`](journal/QAlgo-Dream-G285.json)
- `QAlgo-Dream-G286` → [`QAlgo-Dream-G286.json`](journal/QAlgo-Dream-G286.json)
- `QAlgo-Dream-G287` → [`QAlgo-Dream-G287.json`](journal/QAlgo-Dream-G287.json)
- `QAlgo-Dream-G288` → [`QAlgo-Dream-G288.json`](journal/QAlgo-Dream-G288.json)
- `QAlgo-Dream-G289` → [`QAlgo-Dream-G289.json`](journal/QAlgo-Dream-G289.json)
- `QAlgo-Dream-G290` → [`QAlgo-Dream-G290.json`](journal/QAlgo-Dream-G290.json)
- `QAlgo-Dream-G291` → [`QAlgo-Dream-G291.json`](journal/QAlgo-Dream-G291.json)
- `QAlgo-Dream-G292` → [`QAlgo-Dream-G292.json`](journal/QAlgo-Dream-G292.json)
- `QAlgo-Dream-G293` → [`QAlgo-Dream-G293.json`](journal/QAlgo-Dream-G293.json)
- `QAlgo-Dream-G294` → [`QAlgo-Dream-G294.json`](journal/QAlgo-Dream-G294.json)
- `QAlgo-Dream-G295` → [`QAlgo-Dream-G295.json`](journal/QAlgo-Dream-G295.json)
- `QAlgo-Dream-G296` → [`QAlgo-Dream-G296.json`](journal/QAlgo-Dream-G296.json)
- `QAlgo-Dream-G297` → [`QAlgo-Dream-G297.json`](journal/QAlgo-Dream-G297.json)
- `QAlgo-Dream-G298` → [`QAlgo-Dream-G298.json`](journal/QAlgo-Dream-G298.json)
- `QAlgo-Dream-G299` → [`QAlgo-Dream-G299.json`](journal/QAlgo-Dream-G299.json)
- `QAlgo-Dream-G300` → [`QAlgo-Dream-G300.json`](journal/QAlgo-Dream-G300.json)
- `QAlgo-Dream-G301` → [`QAlgo-Dream-G301.json`](journal/QAlgo-Dream-G301.json)
- `QAlgo-Dream-G302` → [`QAlgo-Dream-G302.json`](journal/QAlgo-Dream-G302.json)
- `QAlgo-Dream-G303` → [`QAlgo-Dream-G303.json`](journal/QAlgo-Dream-G303.json)
- `QAlgo-Dream-G304` → [`QAlgo-Dream-G304.json`](journal/QAlgo-Dream-G304.json)
- `QAlgo-Dream-G305` → [`QAlgo-Dream-G305.json`](journal/QAlgo-Dream-G305.json)
- `QAlgo-Dream-G306` → [`QAlgo-Dream-G306.json`](journal/QAlgo-Dream-G306.json)
- `QAlgo-Dream-G307` → [`QAlgo-Dream-G307.json`](journal/QAlgo-Dream-G307.json)
- `QAlgo-Dream-G308` → [`QAlgo-Dream-G308.json`](journal/QAlgo-Dream-G308.json)
- `QAlgo-Dream-G309` → [`QAlgo-Dream-G309.json`](journal/QAlgo-Dream-G309.json)
- `QAlgo-Dream-G310` → [`QAlgo-Dream-G310.json`](journal/QAlgo-Dream-G310.json)
- `QAlgo-Dream-G311` → [`QAlgo-Dream-G311.json`](journal/QAlgo-Dream-G311.json)
- `QAlgo-Dream-G312` → [`QAlgo-Dream-G312.json`](journal/QAlgo-Dream-G312.json)
- `QAlgo-Dream-G313` → [`QAlgo-Dream-G313.json`](journal/QAlgo-Dream-G313.json)
- `QAlgo-Dream-G314` → [`QAlgo-Dream-G314.json`](journal/QAlgo-Dream-G314.json)
- `QAlgo-Dream-G315` → [`QAlgo-Dream-G315.json`](journal/QAlgo-Dream-G315.json)
- `QAlgo-Dream-G316` → [`QAlgo-Dream-G316.json`](journal/QAlgo-Dream-G316.json)
- `QAlgo-Dream-G317` → [`QAlgo-Dream-G317.json`](journal/QAlgo-Dream-G317.json)
- `QAlgo-Dream-G318` → [`QAlgo-Dream-G318.json`](journal/QAlgo-Dream-G318.json)
- `QAlgo-Dream-G319` → [`QAlgo-Dream-G319.json`](journal/QAlgo-Dream-G319.json)
- `QAlgo-Dream-G320` → [`QAlgo-Dream-G320.json`](journal/QAlgo-Dream-G320.json)
- `QAlgo-Dream-G321` → [`QAlgo-Dream-G321.json`](journal/QAlgo-Dream-G321.json)
- `QAlgo-Dream-G322` → [`QAlgo-Dream-G322.json`](journal/QAlgo-Dream-G322.json)
- `QAlgo-Dream-G323` → [`QAlgo-Dream-G323.json`](journal/QAlgo-Dream-G323.json)
- `QAlgo-Dream-G324` → [`QAlgo-Dream-G324.json`](journal/QAlgo-Dream-G324.json)
- `QAlgo-Dream-G325` → [`QAlgo-Dream-G325.json`](journal/QAlgo-Dream-G325.json)
- `QAlgo-Dream-G326` → [`QAlgo-Dream-G326.json`](journal/QAlgo-Dream-G326.json)
- `QAlgo-Dream-G327` → [`QAlgo-Dream-G327.json`](journal/QAlgo-Dream-G327.json)
- `QAlgo-Dream-G328` → [`QAlgo-Dream-G328.json`](journal/QAlgo-Dream-G328.json)
- `QAlgo-Dream-G329` → [`QAlgo-Dream-G329.json`](journal/QAlgo-Dream-G329.json)
- `QAlgo-Dream-G330` → [`QAlgo-Dream-G330.json`](journal/QAlgo-Dream-G330.json)

## Fitness History

```
G0301 I0: score=2240119
G0302 I0: score=2240110
G0303 I1: score=2240095
G0304 I2: score=2240076
G0305 I0: score=2240104
G0306 I1: score=2240094
G0307 I0: score=2240103
G0308 I1: score=2240077
G0309 I2: score=2240069
G0310 I0: score=2240085
G0311 I2: score=2240069
G0312 I0: score=2240084
G0313 I2: score=2240060
G0314 I0: score=2240070
G0315 I1: score=2240069
G0316 I2: score=2240054
G0317 I1: score=2240060
G0318 I2: score=2240054
G0319 I1: score=2240051
G0320 I2: score=2240043
G0321 I0: score=2240067
G0322 I1: score=2240044
G0323 I2: score=2240043
G0324 I2: score=2240034
G0325 I2: score=2240024
G0326 I0: score=2240055
G0327 I1: score=2240038
G0328 I0: score=2240053
G0329 I1: score=2240029
G0330 I0: score=2240050
```
