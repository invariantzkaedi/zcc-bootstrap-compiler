import os
import time
import subprocess
import threading
from web3 import Web3

print("\033[38;5;17m[MAINFRAME SYNCHRONIZED. DARK FOREST SIMULATOR ACTIVE.]\033[0m")

MAINNET_RPC = os.getenv("ETH_MAINNET_RPC", "http://localhost:8545")
ANVIL_PORT = 8545

def spin_up_anvil_fork():
    print("\033[38;5;51m[SIMULATION]\033[0m Forking Ethereum Mainnet into Local RAM...")
    # Spins up a local Anvil node, forking mainnet, dropping block time to 1 second for rapid testing
    return subprocess.Popen(
        ["anvil", "--fork-url", MAINNET_RPC, "--port", str(ANVIL_PORT), "--block-time", "1"],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL
    )

def inject_synthetic_volatility(w3):
    """
    Simulates competing MEV searchers by spamming the local mempool
    with dummy transactions. This triggers your Nash Equilibrium engine 
    to react to artificial H_t spikes.
    """
    print("\033[38;5;199m[VOLATILITY]\033[0m Injecting synthetic competitor traffic...")
    accounts = w3.eth.accounts
    whale = accounts[0]
    
    while True:
        try:
            # Spam 10-50 transactions per second to simulate block density
            for _ in range(15):
                w3.eth.send_transaction({
                    "from": whale,
                    "to": accounts[1],
                    "value": 1,
                    "gas": 21000,
                    "maxFeePerGas": w3.to_wei(50, 'gwei'),
                    "maxPriorityFeePerGas": w3.to_wei(2, 'gwei')
                })
            time.sleep(1)
        except Exception:
            break

if __name__ == "__main__":
    anvil_proc = spin_up_anvil_fork()
    time.sleep(3) # Wait for Anvil to boot
    
    w3 = Web3(Web3.HTTPProvider(f"http://127.0.0.1:{ANVIL_PORT}"))
    if w3.is_connected():
        print("\033[38;5;51m[MATRIX GREEN]\033[0m Local Dark Forest is live.")
        
        # Run volatility injection in the background
        threading.Thread(target=inject_synthetic_volatility, args=(w3,), daemon=True).start()
        
        try:
            print("\033[38;5;51m[AWAITING STRIKE]\033[0m Point the Omni-Strike pipeline at ws://127.0.0.1:8545")
            while True: time.sleep(1)
        except KeyboardInterrupt:
            print("\n\033[38;5;199m[SHUTDOWN]\033[0m Collapsing local matrix.")
            anvil_proc.terminate()
