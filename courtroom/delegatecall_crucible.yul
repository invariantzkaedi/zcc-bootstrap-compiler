// ==============================================================================
// ZCC DELEGATECALL STORAGE CORRUPTION CRUCIBLE
// AESTHETIC: DEEP NAVY / CYAN & MAGENTA NEON // SPACE MONO
// ==============================================================================
object "DelegateCallCrucible" {
    code {
        // --- HOST CONTEXT ---
        // Let's assume Slot 0 is the "owner" address of the host contract.
        // The host passes its own address (or some critical data) into the delegatecall
        let host_address := address() 
        let target_contract := calldataload(0) // Untrusted target from calldata

        // Store the target in memory for the call
        mstore(0x80, target_contract)
        
        // The Permeable Membrane: DELEGATECALL
        // Retains host storage, msg.sender, msg.value. 
        // IR_TAINT_PROPAGATE must tag 'target_contract' and the execution context.
        let success := delegatecall(gas(), target_contract, 0x80, 0x20, 0x00, 0x00)
        
        if iszero(success) { revert(0, 0) }
        return(0, 0)
    }

    // --- FOREIGN CONTEXT (Simulated execution within the delegatecall) ---
    // In reality, this is external bytecode, but we compile it into the same
    // IR module to test the cross-boundary taint propagation.
    object "ForeignTarget" {
        code {
            // The attacker wants to overwrite Slot 0 (the host's owner).
            // They obfuscate the slot key to bypass naive static analysis.
            
            let base_slot := 0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF
            let counter := 0
            
            // Loop to obfuscate the derivation of Slot 0
            for { } lt(counter, 256) { counter := add(counter, 1) } {
                // Shift the bits until base_slot becomes 0
                base_slot := shl(1, base_slot)
            }
            
            // base_slot is now mathematically 0, but derived through a loop.
            // Mem2Reg will promote 'base_slot' to a phi node. 
            // The taint pass MUST follow the execution context taint into this store.
            
            let malicious_owner := caller() // This is the host's msg.sender!
            
            // The fatal blow. 
            // Because we are in a DELEGATECALL context, this writes to the HOST'S Slot 0.
            // EXPECTED: IR_VULN_PRIV_BOUNDARY | IR_VULN_STATE_WRITE
            sstore(base_slot, malicious_owner)
            
            return(0, 0)
        }
    }
}
