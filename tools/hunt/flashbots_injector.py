import os
import time
from web3 import Web3
from eth_account.signers.local import LocalAccount
from flashbots import flashbot
from eth_account.account import Account

print("\033[38;5;17m[MAINFRAME SYNCHRONIZED. TEMPORAL INJECTOR ONLINE.]\033[0m")

# Connect to your local Erigon/Reth node and the Flashbots Relay
w3 = Web3(Web3.HTTPProvider("http://localhost:8545"))
flashbot(w3, Account.from_key(os.getenv("FLASHBOTS_SIGNATURE_KEY")), "https://relay.flashbots.net")

sender: LocalAccount = Account.from_key(os.getenv("ZKAEDI_OPERATIONAL_KEY"))

def execute_strike(target_tx_hash: str, zkaedi_payload_bytes: bytes, target_block: int):
    print(f"\033[38;5;51m[FLASHBOTS]\033[0m Assembling Bundle for Block {target_block}...")
    
    # 1. Sign your compiled Yul payload
    signed_exploit_tx = w3.eth.account.sign_transaction({
        "nonce": w3.eth.get_transaction_count(sender.address),
        "to": "0xOMNISTRIKE_CONTRACT",
        "value": 0,
        "gas": 300000,
        "maxFeePerGas": w3.to_wei(50, 'gwei'),
        "maxPriorityFeePerGas": 0, # Paid via smart contract coinbase() transfer
        "data": zkaedi_payload_bytes,
        "chainId": 1
    }, sender.key)

    # 2. Construct the Bundle (Target TX + Your TX)
    bundle = [
        {"signed_transaction": target_tx_hash}, # The victim's transaction
        {"signed_transaction": signed_exploit_tx.rawTransaction} # Your strike
    ]

    # 3. Simulate the Bundle before firing
    try:
        simulation = w3.flashbots.simulate(bundle, target_block)
        if 'error' in simulation:
            print(f"\033[38;5;199m[ABORT]\033[0m Simulation Failed: {simulation['error']}")
            return False
            
        print(f"\033[38;5;51m[SIMULATION GREEN]\033[0m Profit verified. Injecting to Relay...")
        
        # 4. Fire the Bundle
        send_result = w3.flashbots.send_bundle(bundle, target_block)
        send_result.wait()
        
        receipts = send_result.receipts()
        print(f"\033[38;5;51m[SUCCESS]\033[0m Bundle mined in block {receipts[0].blockNumber}!")
        return True
        
    except Exception as e:
        print(f"\033[38;5;199m[CRITICAL]\033[0m Relay communication failure: {e}")
        return False
