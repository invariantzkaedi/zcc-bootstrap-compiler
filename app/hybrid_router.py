from __future__ import annotations
from dataclasses import dataclass
from typing import Any, Literal

Route = Literal["retrieval", "tabular", "encoder_then_retrieval", "reject", "neural_inference"]

@dataclass(frozen=True)
class RouteDecision:
    route: Route
    reason: str

_SESS_INT8 = None
_SESS_FP32 = None

def get_onnx_sessions():
    global _SESS_INT8, _SESS_FP32
    import onnxruntime as ort
    from pathlib import Path
    
    int8_name = "ZKAEDI_OMEGA_EDGE_int8.onnx"
    fp32_name = "ZKAEDI_OMEGA_EDGE_fp32.onnx"
    
    paths_to_try = [Path("."), Path(".."), Path("g:/zccMAIN/zcc"), Path("/mnt/g/zccMAIN/zcc")]
    int8_path = None
    fp32_path = None
    
    for p in paths_to_try:
        if (p / int8_name).exists():
            int8_path = p / int8_name
        if (p / fp32_name).exists():
            fp32_path = p / fp32_name
            
    if not int8_path or not fp32_path:
        int8_path = Path("g:/zccMAIN/zcc") / int8_name
        fp32_path = Path("g:/zccMAIN/zcc") / fp32_name
        
    if _SESS_INT8 is None:
        _SESS_INT8 = ort.InferenceSession(str(int8_path), providers=["CPUExecutionProvider"])
    if _SESS_FP32 is None:
        _SESS_FP32 = ort.InferenceSession(str(fp32_path), providers=["CPUExecutionProvider"])
        
    return _SESS_INT8, _SESS_FP32

def cosine_similarity(a, b) -> float:
    import numpy as np
    a_flat = a.flatten()
    b_flat = b.flatten()
    denom = np.linalg.norm(a_flat) * np.linalg.norm(b_flat)
    if denom == 0:
        return 0.0
    return float(np.dot(a_flat, b_flat) / denom)

def map_vector_to_inputs(vector: list[float]) -> dict[str, Any]:
    import numpy as np
    vec = np.array(vector, dtype=np.float32)
    # Average every 3 consecutive elements to shrink 384-D -> 128-D
    mapped_floats = [float(np.mean(vec[i:i+3])) for i in range(0, 384, 3)]
    
    # Scale from [-1.0, 1.0] (typical normalized embedding bounds) to [0, 50264]
    input_ids = np.clip(((np.array(mapped_floats) + 1.0) / 2.0) * 50264, 0, 50264).astype(np.int64)[None, :]
    
    # Use overall mean as h_bias [1, 1]
    h_bias = np.array([[float(np.mean(vec))]], dtype=np.float32)
    
    return {"input_ids": input_ids, "h_bias": h_bias}

def route(payload: dict[str, Any]) -> RouteDecision:
    if "embedding" in payload:
        vector = payload["embedding"]
        if len(vector) != 384:
            raise ValueError(f"Embedding route requires 384-D vector; got {len(vector)}")
        
        intent = payload.get("intent", "retrieval")
        if intent == "none":
            # 1. Load ONNX sessions
            sess_int8, sess_fp32 = get_onnx_sessions()
            
            # 2. Map 384-D vector to input_ids and h_bias
            inputs = map_vector_to_inputs(vector)
            
            # 3. Route primary request to int8 ONNX runtime (for speed)
            out_int8 = sess_int8.run(None, inputs)[0]
            
            # 4. Route sequentially to fp32 reference model in shadow mode
            out_fp32 = sess_fp32.run(None, inputs)[0]
            
            # 5. Calculate live mean cosine similarity between logits
            cos_sim = cosine_similarity(out_fp32, out_int8)
            
            # 6. If cosine similarity decays below 0.995, log critical alert and trigger failure
            if cos_sim < 0.995:
                err_msg = f"[CRITICAL ONNX DIVERGENCE ALERT] Cosine similarity between INT8 and FP32 outputs fell to {cos_sim:.6f} (below threshold of 0.995)!"
                print(err_msg, flush=True)
                raise RuntimeError(err_msg)
                
            reason = f"Neural Inference route verified via Shadow Inference Governance (mean_cosine_similarity: {cos_sim:.6f})"
            return RouteDecision("neural_inference", reason)
            
        return RouteDecision("retrieval", "384-D embedding routed to FAISS retrieval plane")

    if "features" in payload:
        features = payload["features"]
        if len(features) != 91:
            raise ValueError(f"Classical route requires 91-D tabular features; got {len(features)}")
        return RouteDecision("tabular", "91-D tabular feature vector routed to classical side-channel")

    if "text" in payload:
        return RouteDecision("encoder_then_retrieval", "raw text must be encoded before FAISS retrieval")

    return RouteDecision("reject", "payload lacks text, embedding, or features")

def forbid_embedding_to_feature_schema(vector: list[float]) -> None:
    if len(vector) == 384:
        raise RuntimeError("Forbidden transition: 384-D semantic embedding cannot be used as 91-D tabular schema input")

