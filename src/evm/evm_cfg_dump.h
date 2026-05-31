/*
 * evm_cfg_dump.h — EVM CFG Edge Extractor Public API
 */
#ifndef EVM_CFG_DUMP_H
#define EVM_CFG_DUMP_H

#include <stdio.h>

/*
 * evm_cfg_dump() — Two-pass EVM bytecode CFG extractor.
 *
 * Walks `bc` (raw EVM bytecode, `len` bytes), emits a JSON array of
 * control-flow edges to `out`.  Returns the number of edges emitted,
 * or -1 on allocation failure.
 *
 * Edge schema:
 *   {"src_pc":"0xNN","dst_pc":"0xNN","weight":F,"type":"T","tag":"S"}
 *
 * Types: FALLTHROUGH, JUMP, JUMP_INDIRECT, JUMPI_TAKEN,
 *        JUMPI_TAKEN_INDIRECT, JUMPI_FALLTHROUGH,
 *        STOP, RETURN, REVERT, INVALID, SELFDESTRUCT
 *
 * Tags:  NONE, UNTRUSTED_CALL, DELEGATECALL, CALLCODE,
 *        STATIC_CALL, SSTORE, CREATE, CREATE2, SELFDESTRUCT, SHA3
 */
int evm_cfg_dump(const unsigned char *bc, int len, FILE *out);

#endif /* EVM_CFG_DUMP_H */
