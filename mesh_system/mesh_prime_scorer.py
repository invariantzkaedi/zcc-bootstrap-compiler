import math
import random
import time
import pickle

try:
    import prime_vector_pb2
    HAS_PROTO = True
except ImportError:
    HAS_PROTO = False

class PrimeVector:
    def __init__(self, h=0.0, h0=0.0, eta=0.0, gamma=0.0, epsilon=0.0, beta=0.0, 
                 seed=0, timestamp=0, history=None, metrics=None, context=""):
        self.h = h
        self.h0 = h0
        self.eta = eta
        self.gamma = gamma
        self.epsilon = epsilon
        self.beta = beta
        self.seed = seed
        self.timestamp = timestamp
        self.history = history if history is not None else []
        self.metrics = metrics if metrics is not None else {}
        self.context = context

    def serialize(self) -> bytes:
        if HAS_PROTO:
            try:
                pb_vec = prime_vector_pb2.PrimeVector()
                pb_vec.h = self.h
                pb_vec.h0 = self.h0
                pb_vec.eta = self.eta
                pb_vec.gamma = self.gamma
                pb_vec.epsilon = self.epsilon
                pb_vec.beta = self.beta
                pb_vec.seed = self.seed
                pb_vec.timestamp = self.timestamp
                if self.history:
                    pb_vec.history.extend(self.history)
                if self.metrics:
                    for k, v in self.metrics.items():
                        pb_vec.metrics[k] = v
                pb_vec.context = self.context
                return b"ZPB:" + pb_vec.SerializeToString()
            except Exception as e:
                pass
        
        data = {
            "h": self.h, "h0": self.h0, "eta": self.eta, "gamma": self.gamma,
            "epsilon": self.epsilon, "beta": self.beta, "seed": self.seed,
            "timestamp": self.timestamp, "history": self.history,
            "metrics": self.metrics, "context": self.context
        }
        return b"ZPK:" + pickle.dumps(data)

    @classmethod
    def deserialize(cls, data: bytes):
        if data.startswith(b"ZPB:"):
            if not HAS_PROTO:
                raise ImportError("Cannot deserialize protobuf payload: prime_vector_pb2 not found.")
            pb_vec = prime_vector_pb2.PrimeVector()
            pb_vec.ParseFromString(data[4:])
            return cls(
                h=pb_vec.h, h0=pb_vec.h0, eta=pb_vec.eta, gamma=pb_vec.gamma,
                epsilon=pb_vec.epsilon, beta=pb_vec.beta, seed=pb_vec.seed,
                timestamp=pb_vec.timestamp, history=list(pb_vec.history),
                metrics=dict(pb_vec.metrics), context=pb_vec.context
            )
        elif data.startswith(b"ZPK:"):
            d = pickle.loads(data[4:])
            return cls(
                h=d.get("h", 0.0), h0=d.get("h0", 0.0), eta=d.get("eta", 0.0),
                gamma=d.get("gamma", 0.0), epsilon=d.get("epsilon", 0.0),
                beta=d.get("beta", 0.0), seed=d.get("seed", 0),
                timestamp=d.get("timestamp", 0), history=d.get("history"),
                metrics=d.get("metrics"), context=d.get("context", "")
            )
        else:
            if HAS_PROTO:
                try:
                    pb_vec = prime_vector_pb2.PrimeVector()
                    pb_vec.ParseFromString(data)
                    return cls(
                        h=pb_vec.h, h0=pb_vec.h0, eta=pb_vec.eta, gamma=pb_vec.gamma,
                        epsilon=pb_vec.epsilon, beta=pb_vec.beta, seed=pb_vec.seed,
                        timestamp=pb_vec.timestamp, history=list(pb_vec.history),
                        metrics=dict(pb_vec.metrics), context=pb_vec.context
                    )
                except Exception:
                    pass
            d = pickle.loads(data)
            if isinstance(d, dict):
                return cls(
                    h=d.get("h", 0.0), h0=d.get("h0", 0.0), eta=d.get("eta", 0.0),
                    gamma=d.get("gamma", 0.0), epsilon=d.get("epsilon", 0.0),
                    beta=d.get("beta", 0.0), seed=d.get("seed", 0),
                    timestamp=d.get("timestamp", 0), history=d.get("history"),
                    metrics=d.get("metrics"), context=d.get("context", "")
                )
            return d

class PrimeConsensus:
    def __init__(self, state=None, consensus_score=0.0, drift=0.0, jackpot=0, alerts=None):
        self.state = state
        self.consensus_score = consensus_score
        self.drift = drift
        self.jackpot = jackpot
        self.alerts = alerts if alerts is not None else []

    def serialize(self) -> bytes:
        if HAS_PROTO:
            try:
                pb_con = prime_vector_pb2.PrimeConsensus()
                if self.state:
                    pb_con.state.h = self.state.h
                    pb_con.state.h0 = self.state.h0
                    pb_con.state.eta = self.state.eta
                    pb_con.state.gamma = self.state.gamma
                    pb_con.state.epsilon = self.state.epsilon
                    pb_con.state.beta = self.state.beta
                    pb_con.state.seed = self.state.seed
                    pb_con.state.timestamp = self.state.timestamp
                    if self.state.history:
                        pb_con.state.history.extend(self.state.history)
                    if self.state.metrics:
                        for k, v in self.state.metrics.items():
                            pb_con.state.metrics[k] = v
                    pb_con.state.context = self.state.context
                pb_con.consensus_score = self.consensus_score
                pb_con.drift = self.drift
                pb_con.jackpot = self.jackpot
                if self.alerts:
                    pb_con.alerts.extend(self.alerts)
                return b"ZPB_C:" + pb_con.SerializeToString()
            except Exception as e:
                pass

        state_data = {
            "h": self.state.h, "h0": self.state.h0, "eta": self.state.eta,
            "gamma": self.state.gamma, "epsilon": self.state.epsilon,
            "beta": self.state.beta, "seed": self.state.seed,
            "timestamp": self.state.timestamp, "history": self.state.history,
            "metrics": self.state.metrics, "context": self.state.context
        } if self.state else None
        data = {
            "state": state_data,
            "consensus_score": self.consensus_score,
            "drift": self.drift,
            "jackpot": self.jackpot,
            "alerts": self.alerts
        }
        return b"ZPK_C:" + pickle.dumps(data)

    def serialize_c_struct(self) -> bytes:
        import struct
        if not self.state:
            raise ValueError("State is missing")
        
        history = list(self.state.history)
        history_count = len(history)
        if len(history) < 128:
            history += [0.0] * (128 - len(history))
        elif len(history) > 128:
            history = history[:128]
            
        metrics = self.state.metrics or {}
        a_score = metrics.get("agent_a_score", 0.0)
        b_score = metrics.get("agent_b_score", 0.0)
        c_score = metrics.get("agent_c_score", 0.0)
        
        context_bytes = self.state.context.encode('utf-8')[:127]
        context_bytes += b'\x00' * (128 - len(context_bytes))
        
        alerts_str = ",".join(self.alerts)
        alerts_bytes = alerts_str.encode('utf-8')[:511]
        alerts_bytes += b'\x00' * (512 - len(alerts_bytes))
        
        fmt = "@6d2Q128dQ3d128s2dI512s"
        packed = struct.pack(
            fmt,
            self.state.h, self.state.h0, self.state.eta, self.state.gamma, self.state.epsilon, self.state.beta,
            self.state.seed, self.state.timestamp,
            *history,
            history_count,
            a_score, b_score, c_score,
            context_bytes,
            self.consensus_score, self.drift, self.jackpot,
            alerts_bytes
        )
        return b"ZCS:" + packed

    @classmethod
    def deserialize(cls, data: bytes):
        if data.startswith(b"ZCS:"):
            import struct
            payload = data[4:]
            fmt = "@6d2Q128dQ3d128s2dI512s"
            struct_size = struct.calcsize(fmt)
            unpacked = struct.unpack(fmt, payload[:struct_size])
            
            h, h0, eta, gamma, epsilon, beta, seed, timestamp = unpacked[0:8]
            history = list(unpacked[8:136])
            history_count = unpacked[136]
            history = history[:history_count]
            agent_a_score, agent_b_score, agent_c_score = unpacked[137:140]
            context = unpacked[140].split(b'\x00')[0].decode('utf-8', errors='ignore')
            consensus_score, drift, jackpot = unpacked[141:144]
            alerts_raw = unpacked[144].split(b'\x00')[0].decode('utf-8', errors='ignore')
            alerts = [a.strip() for a in alerts_raw.split(',') if a.strip()]
            
            state = PrimeVector(
                h=h, h0=h0, eta=eta, gamma=gamma, epsilon=epsilon, beta=beta,
                seed=seed, timestamp=timestamp, history=history,
                metrics={"agent_a_score": agent_a_score, "agent_b_score": agent_b_score, "agent_c_score": agent_c_score},
                context=context
            )
            return cls(
                state=state, consensus_score=consensus_score, drift=drift,
                jackpot=jackpot, alerts=alerts
            )
        elif data.startswith(b"ZPB_C:"):
            if not HAS_PROTO:
                raise ImportError("Cannot deserialize protobuf payload: prime_vector_pb2 not found.")
            pb_con = prime_vector_pb2.PrimeConsensus()
            pb_con.ParseFromString(data[6:])
            state = None
            if pb_con.HasField("state"):
                state = PrimeVector(
                    h=pb_con.state.h, h0=pb_con.state.h0, eta=pb_con.state.eta,
                    gamma=pb_con.state.gamma, epsilon=pb_con.state.epsilon,
                    beta=pb_con.state.beta, seed=pb_con.state.seed,
                    timestamp=pb_con.state.timestamp, history=list(pb_con.state.history),
                    metrics=dict(pb_con.state.metrics), context=pb_con.state.context
                )
            return cls(
                state=state, consensus_score=pb_con.consensus_score,
                drift=pb_con.drift, jackpot=pb_con.jackpot, alerts=list(pb_con.alerts)
            )
        elif data.startswith(b"ZPK_C:"):
            d = pickle.loads(data[6:])
            state_data = d.get("state")
            state = None
            if state_data:
                state = PrimeVector(
                    h=state_data.get("h", 0.0), h0=state_data.get("h0", 0.0),
                    eta=state_data.get("eta", 0.0), gamma=state_data.get("gamma", 0.0),
                    epsilon=state_data.get("epsilon", 0.0), beta=state_data.get("beta", 0.0),
                    seed=state_data.get("seed", 0), timestamp=state_data.get("timestamp", 0),
                    history=state_data.get("history"), metrics=state_data.get("metrics"),
                    context=state_data.get("context", "")
                )
            return cls(
                state=state, consensus_score=d.get("consensus_score", 0.0),
                drift=d.get("drift", 0.0), jackpot=d.get("jackpot", 0),
                alerts=d.get("alerts")
            )
        else:
            if HAS_PROTO:
                try:
                    pb_con = prime_vector_pb2.PrimeConsensus()
                    pb_con.ParseFromString(data)
                    state = None
                    if pb_con.HasField("state"):
                        state = PrimeVector(
                            h=pb_con.state.h, h0=pb_con.state.h0, eta=pb_con.state.eta,
                            gamma=pb_con.state.gamma, epsilon=pb_con.state.epsilon,
                            beta=pb_con.state.beta, seed=pb_con.state.seed,
                            timestamp=pb_con.state.timestamp, history=list(pb_con.state.history),
                            metrics=dict(pb_con.state.metrics), context=pb_con.state.context
                        )
                    return cls(
                        state=state, consensus_score=pb_con.consensus_score,
                        drift=pb_con.drift, jackpot=pb_con.jackpot, alerts=list(pb_con.alerts)
                    )
                except Exception:
                    pass
            d = pickle.loads(data)
            if isinstance(d, dict):
                state_data = d.get("state")
                state = None
                if state_data:
                    state = PrimeVector(
                        h=state_data.get("h", 0.0), h0=state_data.get("h0", 0.0),
                        eta=state_data.get("eta", 0.0), gamma=state_data.get("gamma", 0.0),
                        epsilon=state_data.get("epsilon", 0.0), beta=state_data.get("beta", 0.0),
                        seed=state_data.get("seed", 0), timestamp=state_data.get("timestamp", 0),
                        history=state_data.get("history"), metrics=state_data.get("metrics"),
                        context=state_data.get("context", "")
                    )
                return cls(
                    state=state, consensus_score=d.get("consensus_score", 0.0),
                    drift=d.get("drift", 0.0), jackpot=d.get("jackpot", 0),
                    alerts=d.get("alerts")
                )
            return d

class PrimeScorer:
    """
    ZKAEDI PRIME Sovereign Hamiltonian Evolution Scorer.
    Recursively evaluates system state:
    H_t = H_0 + eta * H_{t-1} * sigma(gamma * H_{t-1}) + epsilon * N(0, 1 + beta * |H_{t-1}|)
    """
    def __init__(self, h_0=1.0, eta=0.15, gamma=1.8, epsilon=0.07, beta=0.4, seed=777):
        self.h_0 = h_0
        self.h = h_0
        self.eta = eta
        self.gamma = gamma
        self.epsilon = epsilon
        self.beta = beta
        self.seed = seed
        random.seed(seed)

    def sigma(self, x):
        """Sigmoid activation function with overflow protection."""
        try:
            return 1.0 / (1.0 + math.exp(-x))
        except OverflowError:
            return 0.0 if x < 0 else 1.0

    def step(self, h_prev):
        """Advances the state vector by one step using the Hamiltonian drift equation."""
        variance = 1.0 + self.beta * abs(h_prev)
        u1 = random.random()
        u2 = random.random()
        while u1 == 0:
            u1 = random.random()
        normal = math.sqrt(-2.0 * math.log(u1)) * math.cos(2.0 * math.pi * u2)
        
        stochastic_term = self.epsilon * normal * math.sqrt(variance)
        momentum_term = self.eta * h_prev * self.sigma(self.gamma * h_prev)
        
        h_next = self.h_0 + momentum_term + stochastic_term
        
        if h_next > 1e6:
            h_next = 1e6
        elif h_next < -1e6:
            h_next = -1e6
            
        self.h = h_next
        return h_next

    def evolve(self, input_state=1.0):
        """Advances state vector based on external inputs."""
        self.h = self.step(self.h)
        return self.h

    def score_mesh(self, agent_a_score, agent_b_score, agent_c_score, context="mesh_orchestrator"):
        """Consensus mesh evaluator returning a PrimeConsensus struct."""
        combined = (agent_a_score + agent_b_score + agent_c_score) / 3.0
        evolved = self.step(combined)
        drift = abs(evolved - combined)
        
        jackpot_raw = int(evolved * 777 + (777 * math.sin(evolved)) + 42) & 0xFFFFFFFF
        
        alerts = []
        if drift > 0.5:
            alerts.append(f"WARNING: High Hamiltonian Drift detected: {drift:.4f}")
        if jackpot_raw % 77 == 0:
            alerts.append("JACKPOT: Swarm lucky state alignment reached!")

        metrics = {
            "agent_a_score": agent_a_score,
            "agent_b_score": agent_b_score,
            "agent_c_score": agent_c_score
        }
        
        vector_state = PrimeVector(
            h=evolved,
            h0=self.h_0,
            eta=self.eta,
            gamma=self.gamma,
            epsilon=self.epsilon,
            beta=self.beta,
            seed=self.seed,
            timestamp=int(time.time()),
            history=[combined, evolved],
            metrics=metrics,
            context=context
        )
        
        return PrimeConsensus(
            state=vector_state,
            consensus_score=evolved,
            drift=drift,
            jackpot=jackpot_raw,
            alerts=alerts
        )

    def simulate_trajectory(self, h_init, steps=100):
        """Generates a complete trajectory log of states."""
        trajectory = [h_init]
        h_t = h_init
        for _ in range(steps):
            h_t = self.step(h_t)
            trajectory.append(h_t)
        return trajectory
