#include <stdint.h>
#include <stdbool.h>
#define MAX_OPERANDS 4
#define MAX_CALL_ARGS 16
#define MAX_PHI_SOURCES 32
#define MAX_SUCCS 2048
#define MAX_PREDS 2048
#define MAX_INSTRS 65536 /* must exceed max RegID in any compiled function */
#define MAX_BLOCKS                                                             \
  32768 /* per-function block limit (parser/lexer can be large) */
#define MAX_ALLOCS 256
#define MAX_LOOPS 64
#define MAX_LOOP_BLOCKS 256
#define NAME_LEN 64

#define NO_BLOCK 0xFFFFFFFFu /* sentinel: absent block ID    */
#define NO_ALLOC 0xFFFFFFFFu /* sentinel: absent alloca ID   */

/* ── CG-IR-015: IR value type for width-sensitive instruction lowering ────────
 * Propagated from the AST (via member_size encoding) through zcc_lower_expr
 * into Instr.ir_type.  Zero-value IR_TY_I64 is the safe 64-bit default so
 * all existing calloc'd Instrs remain correct without explicit initialisation.
 * Only I32/U32 need special treatment in ir_asm_lower_insn (div/mod/shr).   */
typedef enum {
    IR_TY_I64 = 0, /* signed 64-bit  — long, pointer; also "unknown" default  */
    IR_TY_I32 = 1, /* signed 32-bit  — int                                    */
    IR_TY_U32 = 2, /* unsigned 32-bit — unsigned int                          */
    IR_TY_U64 = 3, /* unsigned 64-bit — unsigned long                         */
} IRType;

typedef uint32_t RegID;   /* virtual register identifier            */
typedef uint32_t BlockID; /* basic block identifier                 */
typedef uint32_t InstrID; /* instruction identifier within a block  */

typedef enum {
  ALIAS_UNKNOWN = 0,
  ALIAS_LOCAL_STACK = 1
} AliasClass;

typedef enum {
  OP_NOP = 0,
  OP_CONST, /* immediate constant (Phase B lowering)           */
  OP_PHI,   /* φ(r₀:B₀, r₁:B₁, …)                */
  OP_ADD,
  OP_SUB,
  OP_MUL,
  OP_DIV,
  OP_MOD, /* TODO: x86 idiv sequence when lowering to asm    */
  OP_BAND,
  OP_BOR,
  OP_BXOR,
  OP_BNOT,
  OP_SHL,
  OP_SHR,
  OP_LT,
  OP_EQ,
  OP_NE,
  OP_GT,
  OP_GE,
  OP_LE, /* comparisons */
  OP_LOAD,
  OP_STORE,
  OP_ALLOCA, /* stack allocation — escape candidate  */
  OP_GEP,    /* GetElementPtr — tracks escape        */
  OP_CALL,   /* may cause escape                     */
  OP_RET,
  OP_BR,     /* unconditional branch                 */
  OP_CONDBR, /* conditional branch                   */
  OP_COPY,
  OP_UNDEF,
  OP_PGO_COUNTER_ADDR, /* PGO instrumentation: dst = &__zcc_edge_counts[imm] */
  OP_GLOBAL,           /* load address of global symbol: lea name(%rip), %reg */
  OP_ASM,              /* inline assembly string */
  OP_VLA_ALLOC,
} Opcode;

static const char *opcode_name[] __attribute__((unused)) = {"nop",
                                                            "const",
                                                            "phi",
                                                            "add",
                                                            "sub",
                                                            "mul",
                                                            "div",
                                                            "mod",
                                                            "band",
                                                            "bor",
                                                            "bxor",
                                                            "bnot",
                                                            "shl",
                                                            "shr",
                                                            "lt",
                                                            "eq",
                                                            "ne",
                                                            "gt",
                                                            "ge",
                                                            "le",
                                                            "load",
                                                            "store",
                                                            "alloca",
                                                            "gep",
                                                            "call",
                                                            "ret",
                                                            "br",
                                                            "condbr",
                                                            "copy",
                                                            "undef",
                                                            "pgo_counter_addr",
                                                            "global",
                                                            "asm"};

typedef struct {
  RegID reg;     /* source register                    */
  BlockID block; /* predecessor block the value comes from */
} PhiSource;

typedef struct Instr {
  InstrID id;
  Opcode op;
  RegID dst; /* destination register (0 = no def)   */
  RegID src[MAX_OPERANDS];
  uint32_t n_src;

  /* φ-node sources (populated only when op == OP_PHI) */
  PhiSource phi[MAX_PHI_SOURCES];
  uint32_t n_phi;

  /* Immediate (OP_CONST, or e.g. alloca size; 0 if unused) */
  int64_t imm;

  /* OP_CALL: callee name and argument registers */
  char call_name[128];
  RegID call_args[MAX_CALL_ARGS];
  int call_args_is_float[MAX_CALL_ARGS];
  uint32_t n_call_args;

  /* Metadata */
  bool dead;        /* marked by DCE                       */
  int is_float;
  bool escape;      /* marked by escape analysis           */
  bool is_param;    /* true if this is a parameter alloca  */
  double exec_freq; /* from PGO profile                    */
  int line_no;      /* source line for DWARF .loc (0 = none) */
  IRType ir_type;   /* CG-IR-015: value type for width-sensitive lowering     */
                    /* (0 = IR_TY_I64 default; set for OP_DIV/MOD/SHR/SHL)   */
  int lscan_seq;    /* Liveness sequence ID injected by ir_asm_number_and_liveness */

  /* Address-Mode Folding (AMF) metadata */
  struct {
    bool folded;      /* true if address-mode fold was matched */
    RegID base;       /* base register ID */
    int64_t disp;     /* displacement offset */
  } amf;

  AliasClass alias_class;

  /* Symbolic Base Tracking (SBT) */
  RegID sbt_base;
  int64_t sbt_offset;
  bool sbt_has_cast;

  char *asm_string;

  struct Instr *next;
  struct Instr *prev;
} Instr;

typedef struct Block {
  BlockID id;
  char name[NAME_LEN];

  /* Instruction doubly-linked list */
  Instr *head;
  Instr *tail;
  uint32_t n_instrs;

  /* CFG edges */
  BlockID succs[MAX_SUCCS];
  uint32_t n_succs;
  BlockID preds[MAX_PREDS];
  uint32_t n_preds;

  /* Branch probabilities (succs[i] → branch_prob[i]) */
  double branch_prob[MAX_SUCCS];

  /* Liveness (DCE / SSA pass) */
  uint64_t live_in[8]; /* bitset — up to 512 regs                */
  uint64_t live_out[8];

  /* PGO / reordering metadata */
  double exec_freq;   /* estimated execution frequency          */
  bool placed;        /* used during chain construction         */
  BlockID chain_next; /* next block in the PGO chain            */

  bool reachable; /* set by CFG reachability pass           */
  uint8_t loop_depth;      /* 0 = not in loop, >0 = nesting depth */
  bool is_loop_header;  /* true if block has an incoming back-edge */
  bool is_pre_header;   /* true if block is the pre-header of a loop */
  int pre_header_of;   /* block index of the loop header this pre-headers */
} Block;

typedef struct Function {
  char name[NAME_LEN];
  char ret_type[16];
  Block *blocks[MAX_BLOCKS];
  uint32_t n_blocks;
  BlockID entry;
  BlockID exit;

  /* Def-use chains (reg → defining instruction) */
  Instr *def_of[MAX_INSTRS];     /* indexed by RegID */
  BlockID def_block[MAX_INSTRS]; /* block housing def_of[r]; NO_BLOCK if arg */
  uint32_t n_regs;

  /* Pass statistics */
  struct {
    uint32_t dce_instrs_removed;
    uint32_t dce_blocks_removed;
    uint32_t ea_promotions;
    uint32_t licm_hoisted;
    uint32_t licm_preheaders_inserted;
    uint32_t pgo_blocks_reordered;
  } stats;
} Function;
