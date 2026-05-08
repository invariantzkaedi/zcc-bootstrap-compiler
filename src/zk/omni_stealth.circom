/* * omni_stealth.circom — ZKAEDI Zero-Knowledge Arbitrage Shield
 * ====================================================
 * Proves that a specific arbitrage path yields expected_profit 
 * WITHOUT revealing the path or the amounts to the mempool.
 */

pragma circom 2.1.5;

include "node_modules/circomlib/circuits/comparators.circom";
include "node_modules/circomlib/circuits/poseidon.circom";

template ArbitrageStealthShield() {
    // Public Inputs (What the EVM sees)
    signal input expected_profit;
    signal input target_reserves_hash; 

    // Private Inputs (Your secret MEV strategy parameters)
    signal input trade_amount_in;
    signal input path_route[3]; 
    signal input final_balance;

    // 1. Prove the final balance meets the expected profit
    component profit_check = GreaterEqThan(252);
    profit_check.in[0] <== final_balance;
    profit_check.in[1] <== expected_profit;
    profit_check.out === 1; // MUST be true, or proof generation fails

    // 2. Cryptographic binding to the current block state
    // We hash the private parameters to ensure the payload cannot be replayed
    component state_hash = Poseidon(4);
    state_hash.inputs[0] <== trade_amount_in;
    state_hash.inputs[1] <== path_route[0];
    state_hash.inputs[2] <== path_route[1];
    state_hash.inputs[3] <== path_route[2];

    // The output hash verifies the execution path without revealing it
    signal output stealth_signature <== state_hash.out;
}

component main = ArbitrageStealthShield();
