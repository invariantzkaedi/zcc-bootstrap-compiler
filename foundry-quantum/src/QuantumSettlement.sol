// SPDX-License-Identifier: MIT
pragma solidity ^0.8.26;

/**
 * @title QuantumSettlement
 * @notice Thin deployment wrapper for the 248-byte pure Yul quantum settlement contract.
 *         All execution, verification, precompiles, and event emissions are executed in pure Yul.
 */
contract QuantumSettlement {
    address public immutable yulContract;

    // The exact 248-byte bytecode produced by solc --strict-assembly on contracts/QuantumSettlement.yul
    bytes constant YUL_BYTECODE = hex"60ec61000c5f3960ec5ff3fe61016c361060e857600435600401602435600401903682113682111760e4573561012882350360e05760201160dc57602001610108816101003760205f61010861010060025afa1560d8576101085f5191013580910360d457801560d057805f541460cc57600180540190600160025401815f55826001556002557f4a9d70e7e179e83df4c944e85cb48ef9df86d7e008cfbf6b22b109e99214b6287f02e541292efc3c324d55e4bb6e85aeaefd31ad80179280405fad0cf9ce25443f925f5260205fa360015f5260205ff35b5f80fd5b5f80fd5b5f80fd5b5f80fd5b5f80fd5b5f80fd5b5f80fd5b5f80fd";

    constructor() {
        bytes memory code = YUL_BYTECODE;
        address addr;
        assembly {
            addr := create(0, add(code, 0x20), mload(code))
            if iszero(addr) { revert(0, 0) }
        }
        yulContract = addr;
    }

    /// @notice Forwarder entry point for verifyAndSettle(bytes,bytes)
    function verifyAndSettle(bytes calldata proof, bytes calldata commitment) external returns (bool) {
        (bool ok, bytes memory ret) = yulContract.call(
            abi.encodeWithSignature("verifyAndSettle(bytes,bytes)", proof, commitment)
        );
        require(ok, "Yul Settlement Reverted");
        return true;
    }
}
