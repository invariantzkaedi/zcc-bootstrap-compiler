// ==============================================================================
// ZCC 256-BIT ARITHMETIC CRUCIBLE
// AESTHETIC: DEEP NAVY / CYAN & MAGENTA NEON // SPACE MONO
// ==============================================================================
object "Uint256Crucible" {
    code {
        // 1. The Overflow Decoy
        let max_uint := 0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF
        let over := add(max_uint, 1) // EVM Truth: 0. 
        
        // 2. The SDIV Sign-Extension Trap
        // In 64-bit, 0xFFFFFFFF is -1 if cast to int, but 4294967295 if uint.
        // EVM Truth: max_uint is -1 in two's complement 256-bit.
        // sdiv(-1, 1) MUST equal -1 (max_uint)
        let one := 1
        let signed_div := sdiv(max_uint, one)
        
        // 3. The 64-bit Boundary Bypass (Multiplication)
        // 2^64 * 2^64 = 2^128 (0 in 64-bit truncation, highly non-zero in EVM)
        let a := 0x10000000000000000
        let b := 0x10000000000000000
        let mult := mul(a, b)
        
        // If the lifter truncates 'mult' to 0, this JUMPI is skipped in the CFG.
        // The Warden will go blind to the subsequent state write.
        if iszero(mult) {
            revert(0, 0)
        }
        
        // IR_VULN_STATE_WRITE MUST trigger here.
        sstore(over, signed_div)
        return(0, 0)
    }
}
