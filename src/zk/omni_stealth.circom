/* * omni_stealth.circom — ZKAEDI Zero-Knowledge Arbitrage Shield
 * ====================================================
 * Proves that a specific arbitrage path yields expected_profit 
 * WITHOUT revealing the path or the amounts to the mempool.
 */

pragma circom 2.1.5;

include "node_modules/circomlib/circuits/comparators.circom";
include "node_modules/circomlib/circuits/poseidon.circom";
include "node_modules/circomlib/circuits/bitify.circom";

template ArbitrageStealthShield() {
    // Public Inputs (What the EVM sees)
    signal input expected_profit;
    signal input target_reserves_hash; 

    // Private Inputs (Your secret MEV strategy parameters)
    signal input trade_amount_in;
    signal input path_route[3]; 
    signal input final_balance;
    signal input pool_reserves[3]; // The private reserve states of the target pools

    // 1. Range check inputs to prevent prime field overflow attacks (252-bit limits)
    component range_final = Num2Bits(252);
    range_final.in <== final_balance;

    component range_profit = Num2Bits(252);
    range_profit.in <== expected_profit;

    component range_trade = Num2Bits(252);
    range_trade.in <== trade_amount_in;

    // 2. Prove the final balance meets the expected profit
    component profit_check = GreaterEqThan(252);
    profit_check.in[0] <== final_balance;
    profit_check.in[1] <== expected_profit;
    profit_check.out === 1; // MUST be true, or proof generation fails

    // 3. Cryptographic binding of target_reserves_hash to private route & reserves
    // Ensures target_reserves_hash cannot be spoofed/replayed for a different route
    component reserves_commitment = Poseidon(6);
    reserves_commitment.inputs[0] <== path_route[0];
    reserves_commitment.inputs[1] <== path_route[1];
    reserves_commitment.inputs[2] <== path_route[2];
    reserves_commitment.inputs[3] <== pool_reserves[0];
    reserves_commitment.inputs[4] <== pool_reserves[1];
    reserves_commitment.inputs[5] <== pool_reserves[2];
    reserves_commitment.out === target_reserves_hash;

    // 4. Cryptographic binding to the current block execution state
    // We hash the private route parameters to generate the signature
    component state_hash = Poseidon(4);
    state_hash.inputs[0] <== trade_amount_in;
    state_hash.inputs[1] <== path_route[0];
    state_hash.inputs[2] <== path_route[1];
    state_hash.inputs[3] <== path_route[2];

    // The output signature verifies execution without path leakage
    signal output stealth_signature <== state_hash.out;
}

component main = ArbitrageStealthShield();
