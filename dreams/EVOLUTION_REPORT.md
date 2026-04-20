# ZCC Oneirogenesis v2 — Evolution Report

**Generated**: 2026-04-20T11:20:07.277735+00:00

## Summary

| Metric | Value |
|--------|-------|
| Global Generation | 50 |
| Total Survived | 55 |
| Total Rejected | 62 |
| Algorithms Discovered | 50 |
| Blacklisted Patterns | 6 |

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

## Fitness History

```
G0021 I3: score=1252916
G0022 I0: score=1252910
G0023 I0: score=1252892
G0024 I3: score=1252910
G0025 I1: score=1252909
G0026 I0: score=1252886
G0027 I1: score=1252902
G0028 I3: score=1252901
G0029 I3: score=1252893
G0030 I0: score=1252876
G0031 I2: score=1252910
G0032 I0: score=1252867
G0033 I0: score=1252858
G0034 I1: score=1252893
G0035 I0: score=1252945
G0036 I1: score=1252946
G0037 I8: score=1252945
G0038 I0: score=1252936
G0039 I1: score=1252938
G0040 I6: score=1252946
G0041 I7: score=1252945
G0042 I0: score=1252919
G0043 I1: score=1252928
G0044 I6: score=1252941
G0045 I7: score=1252945
G0046 I2: score=1252944
G0047 I8: score=1252936
G0048 I1: score=1252919
G0049 I3: score=1252945
G0050 I8: score=1252928
```
