param(
    [string]$DllPath = "H:\__DOWNLOADS\zcc_github_upload\build_win64\zcc_gpu_llm.dll",
    [string]$ModelPath = "H:\models\zkaedi-prime-dpo-q4_k_m.gguf",
    [string]$AotPtxPath = "",
    [string]$Prompt = "What is the capital of France?",
    [int]$MaxSteps = 32,
    [int]$KvType = 0,
    [int]$CtxLen = 0,
    [switch]$Chat,
    [switch]$ResetTest
)

$ErrorActionPreference = "Stop"

$dllFull = (Resolve-Path $DllPath).Path
$modelFull = (Resolve-Path $ModelPath).Path
$ptxFull = $null
if ($AotPtxPath -and $AotPtxPath.Trim() -ne "") {
    $ptxFull = (Resolve-Path $AotPtxPath).Path
}

$code = @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
public delegate void ZccTokenCallback(IntPtr pieceUtf8, int tokenId, IntPtr userdata);

[StructLayout(LayoutKind.Sequential)]
public struct ZccLlmGenConfig {
    public int max_steps;
    public int top_k;
    public int chat_mode;
    public int clean_stream;
    public int ignore_eos;
    public float temp;
    public float top_p;
    public float min_p;
    public float repeat_penalty;
    public uint seed;
    public uint out_cap;
    public uint flags;
    public IntPtr stream_cb;
    public IntPtr userdata;
}

[StructLayout(LayoutKind.Sequential)]
public struct ZccLlmStats {
    public double tok_per_sec;
    public ulong params_count;
    public uint n_layers;
    public uint dim;
    public uint vocab_size;
    public uint eff_max_seq;
    public uint pos;
    public int last_gen_tokens;
    public uint aot_tier;
    public uint kv_type;
}

public static class ZccGpuLlmNative {
    [DllImport("kernel32.dll", CharSet = CharSet.Ansi, SetLastError = true)]
    private static extern IntPtr LoadLibraryA(string lpLibFileName);

    [DllImport("kernel32.dll", SetLastError = true)]
    private static extern bool FreeLibrary(IntPtr hModule);

    [DllImport("zcc_gpu_llm.dll", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    public static extern IntPtr zcc_llm_create(string model_path, string aot_ptx_path, int kv_type, int ctx_len);

    [DllImport("zcc_gpu_llm.dll", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
    public static extern int zcc_llm_generate(IntPtr handle, byte[] prompt_utf8, ref ZccLlmGenConfig cfg, byte[] out_buf);

    [DllImport("zcc_gpu_llm.dll", CallingConvention = CallingConvention.Cdecl)]
    public static extern int zcc_llm_reset(IntPtr handle);

    [DllImport("zcc_gpu_llm.dll", CallingConvention = CallingConvention.Cdecl)]
    public static extern int zcc_llm_get_stats(IntPtr handle, out ZccLlmStats out_stats);

    [DllImport("zcc_gpu_llm.dll", CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr zcc_llm_last_error();

    [DllImport("zcc_gpu_llm.dll", CallingConvention = CallingConvention.Cdecl)]
    public static extern void zcc_llm_destroy(IntPtr handle);

    private static string ReadUtf8Cstr(IntPtr ptr) {
        if (ptr == IntPtr.Zero) return string.Empty;
        int len = 0;
        while (Marshal.ReadByte(ptr, len) != 0) len++;
        if (len == 0) return string.Empty;
        byte[] buf = new byte[len];
        Marshal.Copy(ptr, buf, 0, len);
        return Encoding.UTF8.GetString(buf);
    }

    public static void RunCheck(
        string dllPath,
        string modelPath,
        string aotPtxPath,
        string prompt,
        int maxSteps,
        int kvType,
        int ctxLen,
        bool chatMode,
        bool resetTest
    ) {
        int szCfg = Marshal.SizeOf(typeof(ZccLlmGenConfig));
        int szStats = Marshal.SizeOf(typeof(ZccLlmStats));
        if (szCfg != 64) throw new Exception("Expected sizeof(ZccLlmGenConfig) == 64, got " + szCfg);
        if (szStats != 48) throw new Exception("Expected sizeof(ZccLlmStats) == 48, got " + szStats);

        IntPtr hMod = LoadLibraryA(dllPath);
        if (hMod == IntPtr.Zero) {
            throw new Exception("LoadLibraryA failed for " + dllPath + " win32_err=" + Marshal.GetLastWin32Error());
        }

        IntPtr llm = IntPtr.Zero;
        try {
            llm = zcc_llm_create(modelPath, string.IsNullOrEmpty(aotPtxPath) ? null : aotPtxPath, kvType, ctxLen);
            if (llm == IntPtr.Zero) {
                throw new Exception("zcc_llm_create failed: " + ReadUtf8Cstr(zcc_llm_last_error()));
            }

            List<string> pieces = new List<string>();
            int cbTokens = 0;
            ZccTokenCallback cb = delegate(IntPtr piecePtr, int tokId, IntPtr ud) {
                cbTokens++;
                pieces.Add(ReadUtf8Cstr(piecePtr));
            };

            byte[] outBuf = new byte[16384];
            ZccLlmGenConfig cfg = new ZccLlmGenConfig();
            cfg.max_steps = maxSteps;
            cfg.top_k = 1;
            cfg.chat_mode = chatMode ? 1 : 0;
            cfg.clean_stream = 1;
            cfg.ignore_eos = 0;
            cfg.temp = 0.0f;
            cfg.top_p = 1.0f;
            cfg.min_p = 0.0f;
            cfg.repeat_penalty = 1.0f;
            cfg.seed = 42u;
            cfg.out_cap = (uint)outBuf.Length;
            cfg.flags = 0u;
            cfg.stream_cb = Marshal.GetFunctionPointerForDelegate(cb);
            cfg.userdata = IntPtr.Zero;

            byte[] promptZ = Encoding.UTF8.GetBytes(prompt + "\0");
            int n1 = zcc_llm_generate(llm, promptZ, ref cfg, outBuf);
            GC.KeepAlive(cb);
            if (n1 < 0) {
                throw new Exception("zcc_llm_generate failed: " + ReadUtf8Cstr(zcc_llm_last_error()));
            }
            if (cbTokens <= 0) {
                throw new Exception("Expected cbTokens > 0 during live P/Invoke streaming");
            }
            string text1 = Encoding.UTF8.GetString(outBuf, 0, n1);
            string joined = string.Concat(pieces);
            if (joined != text1) {
                throw new Exception("Streamed callback text != out_buf: [" + joined + "] vs [" + text1 + "]");
            }

            ZccLlmStats st;
            if (zcc_llm_get_stats(llm, out st) != 0) {
                throw new Exception("zcc_llm_get_stats failed");
            }

            Console.WriteLine(string.Format(
                "CSHARP_PINVOKE_STATS layers={0} dim={1} vocab={2} params={3} pos={4} gen={5} cb_toks={6} tok_per_sec={7:F2} aot_tier={8} kv_type={9}",
                st.n_layers, st.dim, st.vocab_size, st.params_count, st.pos, st.last_gen_tokens, cbTokens, st.tok_per_sec, st.aot_tier, st.kv_type
            ));
            Console.WriteLine("CSHARP_PINVOKE_TEXT=" + text1.Replace("\r", "\\r").Replace("\n", "\\n"));

            if (resetTest) {
                if (zcc_llm_reset(llm) != 0) {
                    throw new Exception("zcc_llm_reset failed: " + ReadUtf8Cstr(zcc_llm_last_error()));
                }
                ZccLlmStats stRst;
                if (zcc_llm_get_stats(llm, out stRst) != 0 || stRst.pos != 0) {
                    throw new Exception("Expected pos == 0 after zcc_llm_reset");
                }
                int cb2 = 0;
                ZccTokenCallback cbFn2 = delegate(IntPtr piecePtr, int tokId, IntPtr ud) { cb2++; };
                cfg.stream_cb = Marshal.GetFunctionPointerForDelegate(cbFn2);
                int n2 = zcc_llm_generate(llm, promptZ, ref cfg, outBuf);
                GC.KeepAlive(cbFn2);
                if (n2 < 0 || cb2 <= 0) {
                    throw new Exception("Turn 2 after reset failed");
                }
                ZccLlmStats st2;
                zcc_llm_get_stats(llm, out st2);
                Console.WriteLine("CSHARP_PINVOKE_RESET_OK pos_after_reset=" + stRst.pos + " pos_after_turn2=" + st2.pos + " cb2=" + cb2);
            }
        } finally {
            if (llm != IntPtr.Zero) zcc_llm_destroy(llm);
            FreeLibrary(hMod);
        }
        Console.WriteLine("CSHARP_PINVOKE_OK");
    }
}
'@

if (-not ("ZccGpuLlmNative" -as [type])) {
    Add-Type -TypeDefinition $code -Language CSharp
}

[ZccGpuLlmNative]::RunCheck($dllFull, $modelFull, $ptxFull, $Prompt, $MaxSteps, $KvType, $CtxLen, $Chat.IsPresent, $ResetTest.IsPresent)
