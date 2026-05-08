import math
import sys
import json
import time
import random

class HamiltonianNashEngine:
    def __init__(self, eta=0.4, gamma=1.2, beta=0.1):
        self.H_t = 0.5  # Initial baseline volatility
        self.eta = eta
        self.gamma = gamma
        self.beta = beta
        
    def step_hamiltonian(self) -> float:
        """
        ZKAEDI PRIME recursive sequence:
        H_t = H_{t-1} + eta * H_{t-1} * tanh(gamma * H_{t-1}) + epsilon * N(0, 1 + beta|H_{t-1}|)
        """
        epsilon = random.gauss(0, 1)
        noise = epsilon * math.sqrt(1 + self.beta * abs(self.H_t))
        
        self.H_t = self.H_t + self.eta * self.H_t * math.tanh(self.gamma * self.H_t) + noise
        
        # Constrain to prevent overflow in simulation
        self.H_t = max(-10.0, min(10.0, self.H_t))
        return self.H_t

    def calculate_entropy_competitors(self, mempool_event_rate: float) -> int:
        """
        Calculates dynamic competitor density based on mempool entropy (events/sec).
        """
        base_competitors = 1
        density = math.log1p(mempool_event_rate) * 2.5
        return max(1, int(base_competitors + density + abs(self.H_t)))

    def calculate_optimal_bribe(self, mempool_event_rate: float, expected_value: int, base_fee: int, gas_used: int) -> int:
        # Step the Hamiltonian field
        h_scalar = self.step_hamiltonian()
        
        # Calculate dynamic competitor count
        competitors = self.calculate_entropy_competitors(mempool_event_rate)
        
        # Dynamic risk parameter bound to H_t
        # High volatility -> Low risk parameter -> High Bribe
        risk_aversion = max(0.1, 1.0 / (1.0 + abs(h_scalar)))

        if competitors <= 1:
            return 1 # Monopoly

        net_value = expected_value - (base_fee * gas_used)
        if net_value <= 0:
            return 0
            
        value_per_gas = net_value / gas_used
        
        optimal_priority_fee = value_per_gas * ((competitors - 1) / (competitors - 1 + risk_aversion))
        
        return max(1, int(optimal_priority_fee))

if __name__ == "__main__":
    print("\033[38;5;17m[HAMILTONIAN NASH ENGINE ONLINE]\033[0m")
    
    # Read interactive CLI or WebSocket bridged parameters
    ev = int(sys.argv[1]) if len(sys.argv) > 1 else 500000000000000000 # 0.5 ETH
    base_fee = int(sys.argv[2]) if len(sys.argv) > 2 else 15000000000 # 15 gwei
    gas = int(sys.argv[3]) if len(sys.argv) > 3 else 21000
    mempool_rate = float(sys.argv[4]) if len(sys.argv) > 4 else 150.0 # events/sec
    
    engine = HamiltonianNashEngine()
    
    bribe = engine.calculate_optimal_bribe(mempool_rate, ev, base_fee, gas)
    net_profit = ev - ((base_fee + bribe) * gas)
    
    print(f"\033[38;5;199m[SCALAR]\033[0m H_t Volatility: \033[38;5;51m{engine.H_t:.6f}\033[0m")
    print(f"\033[38;5;199m[ENTROPY]\033[0m Competing Nodes: \033[38;5;51m{engine.calculate_entropy_competitors(mempool_rate)}\033[0m")
    print(f"\033[38;5;51m[BID]\033[0m Optimal Priority Fee: \033[38;5;199m{bribe} wei\033[0m")
    print(f"\033[38;5;51m[PROFIT]\033[0m Projected Margin: \033[38;5;199m{net_profit} wei\033[0m")
    
    with open("/tmp/nash_bribe.json", "w") as f:
        json.dump({
            "h_scalar": engine.H_t,
            "competitors": engine.calculate_entropy_competitors(mempool_rate),
            "bribe_wei": bribe, 
            "net_profit_wei": net_profit
        }, f)
