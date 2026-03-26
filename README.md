# axioma-agent

**Layer 5 — Behavioural Agent** | Axioma Framework

[![DVEC](https://img.shields.io/badge/DVEC-v1.3-blue)](../axioma-spec/docs/dvec/)
[![Determinism](https://img.shields.io/badge/Determinism-D2-green)](../axioma-spec/docs/dvec/)
[![License](https://img.shields.io/badge/License-GPL--3.0-orange)](LICENSE)

## Overview

`axioma-agent` implements the **Agent Totality Contract** for the Axioma framework. It is a total, bounded, deterministic state machine that executes on top of the Layer 6 cryptographic audit substrate.

**System Property**: The agent cannot behave differently without producing different evidence.

## Determinism Classification

| Property | Value |
|----------|-------|
| DVEC Version | 1.3 |
| Determinism Class | D2 — Constrained Deterministic |
| Memory Model | Zero Dynamic Allocation |
| Patent | UK GB2521625.0 |

**D2 Constrained Deterministic** means:
- Given identical initial state
- Given identical ordered sequence of admitted `AX:OBS:v1` inputs
- The agent produces identical `AX:TRANS:v1` sequences

## Health State Machine

```
UNINIT → INIT → ENABLED → ALARM → DEGRADED → STOPPED
                    ↑         ↓         ↓
                    └─────────┴─────────┘
                          (recovery)
```

`STOPPED` is a terminal state — no further transitions without external reset.

## SRS-002 Compliance

This implementation satisfies all 29 SHALL requirements from SRS-002 v0.3:

| Requirement | Description | Status |
|-------------|-------------|--------|
| SHALL-001 | Determinism definition | ✓ |
| SHALL-006 | Pre-commit requirement | ✓ |
| SHALL-007 | Ordering constraint | ✓ |
| SHALL-010 | Monotonicity constraint | ✓ |
| SHALL-013 | Total function | ✓ |
| SHALL-023 | No-Op transition | ✓ |
| SHALL-024 | Fault accumulator | ✓ |
| SHALL-025 | Substrate failure | ✓ |
| SHALL-026 | Genesis binding | ✓ |
| SHALL-027 | Evidence canonicality | ✓ |
| SHALL-028 | Input ordering | ✓ |
| SHALL-029 | Fault accumulator reset | ✓ |

## Building

### Prerequisites

- CMake 3.10+
- C99 compiler (GCC 12+, Clang 12+)
- `axioma-spec` (shared types)
- `axioma-audit` (L6 substrate)

### Build Commands

```bash
cd ~/axilog/axioma-agent
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Run Tests

```bash
ctest --test-dir build --output-on-failure
./build/test_agent_totality
```

### RTM Verification

```bash
python3 ax-rtm-verify.py --root .
```

## API

### Core Functions

```c
/* Initialize agent with ledger genesis */
void ax_agent_init(
    ax_agent_ctx_t   *ctx,
    ct_fault_flags_t *faults
);

/* Bind agent to ledger (genesis verification) */
void ax_agent_bind(
    ax_agent_ctx_t   *ctx,
    ct_fault_flags_t *faults
);

/* Process one input (primary transition engine) */
void ax_agent_step(
    ax_agent_ctx_t   *ctx,
    const ax_input_t *input,
    ct_fault_flags_t *faults
);
```

### Evidence Format

Every state transition produces an `AX:TRANS:v1` record (RFC 8785 canonical JSON):

```json
{"fault_count":0,"input_class":"TIME_OBS","ledger_seq":2,"next_state":"ENABLED","prev_state":"INIT","violation":null}
```

## Golden Reference

Genesis hash (L0) from Phase 1 verification:

```
7bb0d791697306ce2f1cc5df0bcdf66d810d6af9425aa380b352a62453a5ec7b
```

## Test Results

```
axioma-agent: test_agent_totality
DVEC: v1.3 | Layer: L5 | Class: D2
========================================

Initialization Tests:       4/4 PASS
State Transition Tests:     7/7 PASS
Time Monotonicity Tests:    2/2 PASS
Input Ordering Tests:       1/1 PASS
Replay Equivalence Tests:   1/1 PASS
Totality Coverage Tests:    1/1 PASS
No-Op Tests:               1/1 PASS

========================================
Results: 17/17 tests passed
```

## License

Copyright (c) 2026 The Murray Family Innovation Trust

SPDX-License-Identifier: GPL-3.0-or-later

Patent: UK GB2521625.0

## See Also

- [SRS-002 v0.3](../axioma-spec/docs/requirements/SRS-002-v0.3.md) — Agent Totality Specification
- [axioma-audit](../axioma-audit/) — Layer 6 Cryptographic Audit Ledger
- [axioma-spec](../axioma-spec/) — Shared Types and DVEC
