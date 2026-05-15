// ==============================================================================
// ZCC EVM LIFTER CRUCIBLE
// AESTHETIC: DEEP NAVY / CYAN & MAGENTA NEON // SPACE MONO
// ==============================================================================
object "ReentrancyCrucible" {
    code {
        // 1. Setup & SLOAD
        let caller_addr := caller()
        let bal := sload(caller_addr)
        
        // 2. The Untrusted Interaction
        // IR_VULN_UNTRUSTED_CALL expected here
        let success := call(gas(), caller_addr, bal, 0, 0, 0, 0)
        
        // 3. The Execution Barrier
        // ir_vuln_tag MUST identify this conditional REVERT
        if iszero(success) { 
            revert(0, 0) 
        }
        
        // 4. The State Mutation (Vulnerability: Written AFTER the call)
        // IR_VULN_STATE_WRITE expected here
        sstore(caller_addr, 0)
        
        return(0, 0)
    }
}
