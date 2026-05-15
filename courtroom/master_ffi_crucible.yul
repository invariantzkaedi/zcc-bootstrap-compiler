object "MasterFFICrucible" {
    code {
        let caller_addr := caller()
        let bal := sload(caller_addr)                // tainted host context

        // DELEGATECALL with tainted target
        let success := delegatecall(gas(), caller_addr, 0, 0, 0, 0)

        if iszero(success) { revert(0, 0) }

        // 256-bit overflow + gas burn
        let x := 0xffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff
        let y := 1
        let z := add(caller_addr, y)                      // must wrap to 0

        // Final state write with tainted value
        sstore(caller_addr, z)                       // should trigger PRIV_BOUNDARY + OOG check

        return(0, 0)
    }
}
