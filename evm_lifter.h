/*
 * evm_lifter.h — ZCC Defensive EVM Bytecode Lifter Scaffold
 *
 * PURPOSE: Defensive contract-audit analysis scaffold ONLY.
 *   This module lifts raw EVM bytecode into ZCC IR for static analysis.
 *   It does NOT perform exploit automation, network execution, offensive
 *   payload generation, or exploit deployment.
 *
 * DESIGN:
 *   - Bypasses the C AST pipeline in part4.c entirely.  The lifter consumes
 *     raw bytecode bytes and emits IR nodes directly via ir.h / ir.c APIs,
 *     never touching ZCC's token/parse/type-check machinery.
 *   - Aligns with ir_bridge.h virtual register naming conventions (t0, t1,
 *     t2, ...) so lifted IR can be fed to existing compiler pass analysis.
 *   - Security-sensitive opcodes receive ir_node_t.tag values so that future
 *     passes (RegisterWarden, reentrancy liveness, dominance analysis) can
 *     locate them without scanning all opcodes.
 *   - TAG_UNTRUSTED_EXTERNAL_CALL is assigned to CALL, DELEGATECALL, and
 *     CALLCODE — operations that transfer control to arbitrary addresses.
 *   - TAG_STATIC_CALL is assigned to STATICCALL — read-only, lower risk.
 *
 * OPCODE COVERAGE NOTE:
 *   The opcode space is 256 values (0x00–0xFF).  Of these, ~140 are defined
 *   (valid) opcodes in the Yellow Paper + Shanghai EIPs; the rest are INVALID.
 *   The enum below names all ~70 distinct mnemonic groups; PUSH1..PUSH32,
 *   DUP1..DUP16, and SWAP1..SWAP16 are ranges (not individually enumerated).
 *   All 256 byte values are handled by the lifter (invalid opcodes → IR_NOP).
 *
 * COVERAGE NOTE:
 *   This is a scaffold.  The tests in tests/test_evm_lifter.c cover the
 *   critical paths (stepping, PUSH immediates, CALL-family tagging, stack
 *   simulation, truncated/malformed bytecode).  Full 95%+ coverage across
 *   all ~140 EVM opcodes requires a production harness beyond this scaffold.
 *   See the issue tracker for the 95%+ gate requirement before production use.
 *
 * FUTURE WORK:
 *   - Expose tagged nodes to RegisterWarden / liveness / dominance passes.
 *   - Full 256-bit word support (currently truncated to 64-bit, but wide
 *     constants are explicitly tagged with IR_TAG_TRUNCATED_WIDE_CONST).
 *   - More advanced indirect jump resolution (basic direct PUSH+JUMP
 *     targets are now resolved and emitted as IR_BR/IR_BR_IF).
 */

#ifndef ZCC_EVM_LIFTER_H
#define ZCC_EVM_LIFTER_H

#include "ir.h"

/* ── EVM Security Tags ───────────────────────────────────────────────── */
/*
 * These values are stored in ir_node_t.tag.  They mark IR nodes that
 * correspond to security-relevant EVM opcodes for downstream analysis.
 *
 * Consumers: RegisterWarden, reentrancy liveness pass (future),
 *            dominance analysis, audit receipt generation.
 *
 * IR_TAG_NONE == 0 means "no security tag" — safe by default for any
 * node created via ir_node_alloc() (calloc zeroes the tag field).
 */
typedef enum {
    IR_TAG_NONE                    = 0, /* untagged — no security concern       */
    IR_TAG_UNTRUSTED_EXTERNAL_CALL = 1, /* CALL, DELEGATECALL, CALLCODE         */
    IR_TAG_STATIC_CALL             = 2, /* STATICCALL — read-only, lower risk   */
    IR_TAG_SELFDESTRUCT            = 3, /* SELFDESTRUCT — destructive op        */
    IR_TAG_SSTORE                  = 4, /* SSTORE — persistent state write      */
    IR_TAG_CREATE                  = 5, /* CREATE / CREATE2 — contract deploy   */
    IR_TAG_EVM_BARRIER             = 6, /* INVALID / REVERT — execution barrier */
    IR_TAG_TRUNCATED_WIDE_CONST    = 7  /* Wide PUSH truncated to 64-bit        */
} evm_ir_tag_t;

/* ── EVM Opcodes (Ethereum Yellow Paper + Shanghai EIPs) ────────────── */
typedef enum {
    /* 0x00 — stop/arithmetic */
    EVM_STOP          = 0x00,
    EVM_ADD           = 0x01,
    EVM_MUL           = 0x02,
    EVM_SUB           = 0x03,
    EVM_DIV           = 0x04,
    EVM_SDIV          = 0x05,
    EVM_MOD           = 0x06,
    EVM_SMOD          = 0x07,
    EVM_ADDMOD        = 0x08,
    EVM_MULMOD        = 0x09,
    EVM_EXP           = 0x0a,
    EVM_SIGNEXTEND    = 0x0b,

    /* 0x10 — comparison/bitwise */
    EVM_LT            = 0x10,
    EVM_GT            = 0x11,
    EVM_SLT           = 0x12,
    EVM_SGT           = 0x13,
    EVM_EQ            = 0x14,
    EVM_ISZERO        = 0x15,
    EVM_AND           = 0x16,
    EVM_OR            = 0x17,
    EVM_XOR           = 0x18,
    EVM_NOT           = 0x19,
    EVM_BYTE          = 0x1a,
    EVM_SHL           = 0x1b,
    EVM_SHR           = 0x1c,
    EVM_SAR           = 0x1d,

    /* 0x20 — hash */
    EVM_SHA3          = 0x20,

    /* 0x30 — environment */
    EVM_ADDRESS       = 0x30,
    EVM_BALANCE       = 0x31,
    EVM_ORIGIN        = 0x32,
    EVM_CALLER        = 0x33,
    EVM_CALLVALUE     = 0x34,
    EVM_CALLDATALOAD  = 0x35,
    EVM_CALLDATASIZE  = 0x36,
    EVM_CALLDATACOPY  = 0x37,
    EVM_CODESIZE      = 0x38,
    EVM_CODECOPY      = 0x39,
    EVM_GASPRICE      = 0x3a,
    EVM_EXTCODESIZE   = 0x3b,
    EVM_EXTCODECOPY   = 0x3c,
    EVM_RETURNDATASIZE= 0x3d,
    EVM_RETURNDATACOPY= 0x3e,
    EVM_EXTCODEHASH   = 0x3f,

    /* 0x40 — block */
    EVM_BLOCKHASH     = 0x40,
    EVM_COINBASE      = 0x41,
    EVM_TIMESTAMP     = 0x42,
    EVM_NUMBER        = 0x43,
    EVM_PREVRANDAO    = 0x44,
    EVM_GASLIMIT      = 0x45,
    EVM_CHAINID       = 0x46,
    EVM_SELFBALANCE   = 0x47,
    EVM_BASEFEE       = 0x48,

    /* 0x50 — stack/memory/storage/flow */
    EVM_POP           = 0x50,
    EVM_MLOAD         = 0x51,
    EVM_MSTORE        = 0x52,
    EVM_MSTORE8       = 0x53,
    EVM_SLOAD         = 0x54,
    EVM_SSTORE        = 0x55,
    EVM_JUMP          = 0x56,
    EVM_JUMPI         = 0x57,
    EVM_PC            = 0x58,
    EVM_MSIZE         = 0x59,
    EVM_GAS           = 0x5a,
    EVM_JUMPDEST      = 0x5b,
    EVM_PUSH0         = 0x5f,  /* EIP-3855 (Shanghai) */

    /* 0x60–0x7f — PUSH1..PUSH32 */
    EVM_PUSH1         = 0x60,
    EVM_PUSH32        = 0x7f,

    /* 0x80–0x8f — DUP1..DUP16 */
    EVM_DUP1          = 0x80,
    EVM_DUP16         = 0x8f,

    /* 0x90–0x9f — SWAP1..SWAP16 */
    EVM_SWAP1         = 0x90,
    EVM_SWAP16        = 0x9f,

    /* 0xa0–0xa4 — LOG0..LOG4 */
    EVM_LOG0          = 0xa0,
    EVM_LOG1          = 0xa1,
    EVM_LOG2          = 0xa2,
    EVM_LOG3          = 0xa3,
    EVM_LOG4          = 0xa4,

    /* 0xf0 — system */
    EVM_CREATE        = 0xf0,
    EVM_CALL          = 0xf1,  /* SECURITY: untrusted external call        */
    EVM_CALLCODE      = 0xf2,  /* SECURITY: untrusted external call        */
    EVM_RETURN        = 0xf3,
    EVM_DELEGATECALL  = 0xf4,  /* SECURITY: untrusted external call        */
    EVM_CREATE2       = 0xf5,
    EVM_STATICCALL    = 0xfa,  /* SECURITY: static (read-only) call        */
    EVM_REVERT        = 0xfd,
    EVM_INVALID       = 0xfe,
    EVM_SELFDESTRUCT  = 0xff   /* SECURITY: destructive                    */
} evm_opcode_t;

/* ── EVM Virtual Stack ───────────────────────────────────────────────── */
/*
 * The EVM stack is modelled with IR temporary names at each slot.
 * EVM spec allows up to 1024 stack items; we enforce this limit.
 */
enum { EVM_STACK_MAX = 1024 };

typedef struct {
    char slots[EVM_STACK_MAX][IR_NAME_MAX]; /* IR temp name at each slot     */
    long const_vals[EVM_STACK_MAX];         /* If known constant, its value  */
    char is_const[EVM_STACK_MAX];           /* 1 if slots[i] is a known const*/
    int  depth;                              /* current depth (0 = empty)     */
} evm_stack_t;

/* ── Lifter Result ───────────────────────────────────────────────────── */
typedef enum {
    EVM_LIFT_OK          = 0, /* instruction lifted successfully           */
    EVM_LIFT_TRUNCATED   = 1, /* bytecode truncated mid-instruction        */
    EVM_LIFT_INVALID_OP  = 2, /* unknown opcode encountered                */
    EVM_LIFT_STACK_OVER  = 3, /* EVM stack would exceed 1024               */
    EVM_LIFT_STACK_UNDER = 4, /* EVM stack underflow (pop from empty)      */
    EVM_LIFT_OOM         = 5  /* IR allocation out of memory               */
} evm_lift_result_t;

/* ── Lifter State ────────────────────────────────────────────────────── */
typedef struct {
    const unsigned char *bytecode;    /* raw EVM bytecode                  */
    int                  length;      /* total byte count                  */
    int                  pc;          /* program counter (byte offset)     */

    evm_stack_t          stack;       /* simulated EVM stack               */

    ir_module_t         *module;      /* IR module being populated         */
    ir_func_t           *func;        /* current IR function               */
    int                  tmp_seq;     /* VReg counter (ir_bridge convention)*/

    int                  error;       /* non-zero after fatal error        */
    char                 errmsg[128]; /* last error message string         */

    int                  insn_count;  /* total instructions stepped        */
    int                  call_count;  /* CALL-family opcodes seen          */
    int                  tagged_count;/* nodes with non-zero security tags */

    unsigned char       *valid_jumpdest; /* bitmap of valid JUMPDEST offsets */
} evm_lifter_t;

/* ── Public API ──────────────────────────────────────────────────────── */

/*
 * Initialize lifter state.  Must be called before evm_lift_bytecode() or
 * evm_lift_step().  `module` must already be created via ir_module_create();
 * a fresh function named "evm_contract" is created inside it.
 */
void evm_lifter_init(evm_lifter_t *ls,
                     const unsigned char *bytecode, int length,
                     ir_module_t *module);

/*
 * Free dynamically allocated resources inside the lifter state.
 */
void evm_lifter_destroy(evm_lifter_t *ls);

/*
 * Lift the entire bytecode stream into IR.
 * Returns EVM_LIFT_OK when the stream is exhausted normally.
 * On error, ls->errmsg describes what went wrong.
 */
evm_lift_result_t evm_lift_bytecode(evm_lifter_t *ls);

/*
 * Step a single EVM instruction starting at ls->pc.
 * Advances ls->pc past the instruction (including immediate bytes).
 * Emits one or more IR nodes into ls->func.
 * Returns EVM_LIFT_OK on success, EVM_LIFT_TRUNCATED if the stream ends
 * in the middle of an instruction's immediate data.
 */
evm_lift_result_t evm_lift_step(evm_lifter_t *ls);

/* Return a human-readable mnemonic for an EVM opcode byte. */
const char *evm_opcode_name(unsigned int opcode);

/* Return a human-readable name for an evm_ir_tag_t value. */
const char *evm_tag_name(evm_ir_tag_t tag);

/*
 * Return 1 if `opcode` is a CALL-family opcode that transfers execution
 * to an external address (CALL, DELEGATECALL, CALLCODE, STATICCALL).
 * Returns 0 otherwise.
 */
int evm_is_call_family(unsigned int opcode);

#endif /* ZCC_EVM_LIFTER_H */
