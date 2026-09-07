# Nexus Legendary Edition

Dependency-free Python 3.11+ symbolic runtime and autonomous verification nervous system.

## Layout

- `nexus/models.py` — immutable frames, checkpoint/replay/supervisor models.
- `nexus/ledger.py` — append-only SHA-256 chained DAG ledger, atomic checkpoint store, verifier.
- `nexus/checkpoint_engine.py` — symbolic drift scoring/classification and checkpoint emission.
- `nexus/replay_engine.py` — deterministic counterfactual DAG forks and empirical causal deltas.
- `nexus/supervisor.py` — bounded auditable supervisor policy with first-class decision frames.
- `nexus/demo.py` — AdamW vs ZKAEDI-Prime illustrative integration demo.
- `tests/test_nexus_legendary.py` — drift math, boundary, lineage, replay, supervisor, and tamper tests.

## Verify

```bash
python -m unittest tests/test_nexus_legendary.py -v
python -m compileall -q nexus tests
python -m nexus.demo
```

## Security model

Each checkpoint is canonically serialized and SHA-256 hashed. DAG lineage uses
`H(parent_lineage_hash || node_uuid.bytes)`. Ledger envelopes form an independent
append-only SHA-256 record chain. Checkpoint files are fsynced and atomically
replaced before their frames are advertised in the ledger.
