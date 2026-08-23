// SPDX-License-Identifier: MIT
pragma solidity ^0.8.26;

import "../src/QuantumSettlement.sol";

contract DeployQuantumSettlement {
    function run() external returns (address) {
        QuantumSettlement settlement = new QuantumSettlement();
        return settlement.yulContract();
    }
}
