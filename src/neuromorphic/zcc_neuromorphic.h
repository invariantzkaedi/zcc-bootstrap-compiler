/* ========================================================================= */
/* ZCC NEUROMORPHIC SPIKING COMPILER & QUANTUM RESERVOIR SUITE (M1-M5)      */
/* ========================================================================= */
/* File: src/neuromorphic/zcc_neuromorphic.h                                 */
/* Description: Complete 5-Milestone Neuromorphic Spiking Architecture:      */
/*              M1: Event-Driven Spiking IR & Vectorized LIF Core            */
/*              M2: Spike-Timing-Dependent Plasticity (STDP) Local Learning  */
/*              M3: Continuous ANN -> SNN Zero-Loss Transpiler               */
/*              M4: Intel Loihi 2 NoC Routing & Microcode Emitter            */
/*              M5: Quantum-Neuromorphic Reservoir Computing Engine          */
/* ========================================================================= */

#ifndef ZCC_NEUROMORPHIC_H
#define ZCC_NEUROMORPHIC_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NEURO_MAX_NEURONS    256
#define NEURO_MAX_SYNAPSES   1024
#define NEURO_MAX_AER_EVENTS 512
#define NEURO_RESERVOIR_DIM  64

/* ------------------------------------------------------------------------- */
/* M1: Spiking IR Data Structures & LIF Core                                 */
/* ------------------------------------------------------------------------- */

/* Asynchronous Address-Event Representation (AER) Packet */
typedef struct {
    uint32_t timestamp_us;
    uint16_t src_neuron_id;
    uint16_t dst_neuron_id;
    float    weight_current;
} NeuroAerEvent;

typedef struct {
    NeuroAerEvent events[NEURO_MAX_AER_EVENTS];
    uint32_t      head;
    uint32_t      tail;
    uint32_t      count;
} NeuroAerQueue;

/* Leaky Integrate-and-Fire (LIF) Neuron Population */
typedef struct {
    uint32_t n_neurons;
    float    v_membrane[NEURO_MAX_NEURONS];   /* Current membrane potential */
    float    v_rest[NEURO_MAX_NEURONS];       /* Resting potential (-70 mV) */
    float    v_threshold[NEURO_MAX_NEURONS];  /* Firing threshold (-50 mV) */
    float    v_reset[NEURO_MAX_NEURONS];      /* Reset potential (-65 mV) */
    float    decay_beta;                      /* Leak factor e^(-dt/tau) */
    uint32_t refractory_timer[NEURO_MAX_NEURONS]; /* Refractory periods */
    uint64_t spike_words[NEURO_MAX_NEURONS / 64 + 1]; /* Bit-packed 64-spike channels */
} NeuroLifPopulation;

/* ------------------------------------------------------------------------- */
/* M2: STDP Plasticity & Homeostasis                                         */
/* ------------------------------------------------------------------------- */

typedef struct {
    uint16_t pre_neuron;
    uint16_t post_neuron;
    float    weight;
    uint32_t last_pre_spike_us;
    uint32_t last_post_spike_us;
} NeuroSynapse;

typedef struct {
    float a_plus;   /* Potentiation scale (e.g. 0.01) */
    float a_minus;  /* Depression scale (e.g. 0.012) */
    float tau_plus; /* Potentiation time constant (20 us) */
    float tau_minus;/* Depression time constant (20 us) */
    float w_min;
    float w_max;
} NeuroStdpConfig;

/* ------------------------------------------------------------------------- */
/* M3: Continuous ANN -> SNN Transpiler Structures                           */
/* ------------------------------------------------------------------------- */

typedef struct {
    uint32_t n_inputs;
    uint32_t n_outputs;
    float    ann_weights[NEURO_MAX_NEURONS * NEURO_MAX_NEURONS];
    float    ann_biases[NEURO_MAX_NEURONS];
    float    v_threshold_calibrated[NEURO_MAX_NEURONS];
    float    rate_coding_scale;
} NeuroAnnTranspilerConfig;

/* ------------------------------------------------------------------------- */
/* M4: Intel Loihi 2 Microcode & NoC Mesh Routing                            */
/* ------------------------------------------------------------------------- */

typedef struct {
    uint8_t  src_core_x;
    uint8_t  src_core_y;
    uint8_t  dst_core_x;
    uint8_t  dst_core_y;
    uint16_t axon_id;
    int8_t   quantized_weight; /* 8-bit integer [-128, 127] */
} NeuroLoihiPacket;

typedef struct {
    uint32_t n_packets;
    NeuroLoihiPacket packets[NEURO_MAX_SYNAPSES];
    double   estimated_power_mw;
    double   mesh_latency_ns;
} NeuroLoihiNoCRoute;

/* ------------------------------------------------------------------------- */
/* M5: Quantum-Neuromorphic Reservoir State                                  */
/* ------------------------------------------------------------------------- */

typedef struct {
    uint32_t reservoir_dim;
    float    state[NEURO_RESERVOIR_DIM];
    float    w_reservoir[NEURO_RESERVOIR_DIM * NEURO_RESERVOIR_DIM];
    float    w_input[NEURO_RESERVOIR_DIM * NEURO_MAX_NEURONS];
    float    w_readout[NEURO_RESERVOIR_DIM];
    float    quantum_coupling_eta;
    float    quantum_phase_theta;
} NeuroQuantumReservoir;

/* ------------------------------------------------------------------------- */
/* FUNCTION PROTOTYPES (M1 - M5)                                             */
/* ------------------------------------------------------------------------- */

/* M1: Initialize and step LIF population */
bool neuro_lif_init(NeuroLifPopulation *pop, uint32_t n_neurons, float decay_beta, float v_thresh);
uint32_t neuro_lif_step(NeuroLifPopulation *pop, const float *input_currents, uint32_t dt_us);
bool neuro_aer_push(NeuroAerQueue *q, NeuroAerEvent evt);
bool neuro_aer_pop(NeuroAerQueue *q, NeuroAerEvent *out_evt);

/* M2: STDP Synaptic Learning Step */
bool neuro_stdp_init_synapse(NeuroSynapse *syn, uint16_t pre, uint16_t post, float init_w);
bool neuro_stdp_apply_event(NeuroSynapse *syn, const NeuroStdpConfig *cfg, uint32_t spike_time_us, bool is_pre_spike);

/* M3: Transpile ANN continuous layer to SNN calibrated threshold */
bool neuro_ann2snn_calibrate(
    const float *dense_weights,
    const float *biases,
    uint32_t n_in,
    uint32_t n_out,
    NeuroAnnTranspilerConfig *out_cfg
);
uint32_t neuro_ann2snn_infer_spikes(
    const NeuroAnnTranspilerConfig *cfg,
    const float *input_activations,
    uint32_t timesteps,
    float *out_rate_estimations
);

/* M4: Synthesize Intel Loihi 2 NoC Mesh Routing & Microcode */
bool neuro_loihi_synthesize_mesh(
    const NeuroSynapse *synapses,
    uint32_t n_synapses,
    NeuroLoihiNoCRoute *out_route
);
int neuro_loihi_emit_assembly_text(const NeuroLoihiNoCRoute *route, char *out_buf, size_t max_len);

/* M5: Quantum-Neuromorphic Reservoir Simulation & Readout */
bool neuro_quantum_reservoir_init(NeuroQuantumReservoir *res, uint32_t dim, float eta);
float neuro_quantum_reservoir_step(
    NeuroQuantumReservoir *res,
    const uint64_t *input_spikes,
    float quantum_hamiltonian_phase
);
bool neuro_quantum_reservoir_train_readout(
    NeuroQuantumReservoir *res,
    const float *states_matrix,
    const float *target_outputs,
    uint32_t n_samples,
    float lambda_reg
);

#ifdef __cplusplus
}
#endif

#endif /* ZCC_NEUROMORPHIC_H */
