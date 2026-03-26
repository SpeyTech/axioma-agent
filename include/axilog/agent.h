/**
 * @file agent.h
 * @brief Axilog Layer 5 Behavioural Agent
 *
 * DVEC: v1.3
 * DETERMINISM: D2 — Constrained Deterministic
 * MEMORY: Zero Dynamic Allocation
 *
 * This module implements the total, bounded, deterministic state machine
 * for the Axioma framework. Every state transition is committed to the
 * L6 audit ledger prior to in-memory mutation.
 *
 * Copyright (c) 2026 The Murray Family Innovation Trust
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Patent: UK GB2521625.0
 *
 * @traceability SRS-002-SHALL-001 through SRS-002-SHALL-029
 */

#ifndef AXILOG_AGENT_H
#define AXILOG_AGENT_H

#include <axilog/types.h>
#include <axilog/audit.h>
#include <stdint.h>

/* ========================================================================
 * HEALTH STATE ENUMERATION (SRS-002-SHALL-003, SRS-002-SHALL-015)
 * ======================================================================== */

/**
 * @brief Agent health states
 *
 * Defines the operational status of the agent. State transitions
 * are governed by the transition matrix (SRS-002 §9.1).
 *
 * Terminal: AX_HEALTH_STOPPED — no further transitions without reset.
 *
 * @traceability SRS-002-SHALL-003, SRS-002-SHALL-015
 */
typedef enum {
    AX_HEALTH_UNINIT   = 0,  /**< Initial state, not bound to ledger */
    AX_HEALTH_INIT     = 1,  /**< Bound to ledger, awaiting temporal sync */
    AX_HEALTH_ENABLED  = 2,  /**< Fully operational */
    AX_HEALTH_ALARM    = 3,  /**< Fault threshold exceeded */
    AX_HEALTH_DEGRADED = 4,  /**< Operating in reduced capacity */
    AX_HEALTH_STOPPED  = 5   /**< Terminal state — no mutation permitted */
} ax_health_state_t;

/* ========================================================================
 * INPUT CLASS ENUMERATION (SRS-002-SHALL-012)
 * ======================================================================== */

/**
 * @brief Admitted input classes
 *
 * The input alphabet is closed — no undeclared types permitted.
 * All inputs MUST be admitted as AX:OBS:v1 before processing.
 *
 * @traceability SRS-002-SHALL-002, SRS-002-SHALL-012
 */
typedef enum {
    AX_INPUT_TIME_OBS      = 0,  /**< Admitted timestamp observation */
    AX_INPUT_LLM_OBS       = 1,  /**< Admitted LLM response observation */
    AX_INPUT_POLICY_TRIGGER = 2, /**< Policy evaluation trigger */
    AX_INPUT_FAULT_SIGNAL  = 3,  /**< Fault condition signal */
    AX_INPUT_RESET_REQUEST = 4   /**< Reset/recovery request */
} ax_input_class_t;

/** @brief Number of defined input classes (for totality coverage) */
#define AX_INPUT_CLASS_COUNT 5

/** @brief Number of defined health states (for totality coverage) */
#define AX_HEALTH_STATE_COUNT 6

/* ========================================================================
 * VIOLATION TYPES (SRS-002-SHALL-020)
 * ======================================================================== */

/**
 * @brief Violation type enumeration
 *
 * All violations result in deterministic state transitions.
 * Critical violations → STOPPED.
 *
 * @traceability SRS-002-SHALL-020
 */
typedef enum {
    AX_VIOLATION_NONE             = 0,  /**< No violation */
    AX_VIOLATION_TIME_ROLLBACK    = 1,  /**< T_new <= T_last */
    AX_VIOLATION_POLICY_BREACH    = 2,  /**< Policy constraint violated */
    AX_VIOLATION_FAULT_EXCEEDED   = 3,  /**< Fault budget exceeded */
    AX_VIOLATION_PROTOCOL         = 4,  /**< State machine protocol error */
    AX_VIOLATION_GENESIS_MISMATCH = 5,  /**< Ledger binding failure */
    AX_VIOLATION_COMMIT_FAILURE   = 6,  /**< L6 commit failed */
    AX_VIOLATION_SEQUENCE_ORDER   = 7,  /**< Input ordering violation */
    AX_VIOLATION_TERMINALITY      = 8   /**< Input to STOPPED state */
} ax_violation_t;

/* ========================================================================
 * INPUT STRUCTURE (SRS-002-SHALL-002, SRS-002-SHALL-028)
 * ======================================================================== */

/**
 * @brief Admitted input record
 *
 * Represents an input that has already been admitted to the L6 ledger
 * as AX:OBS:v1. Direct system calls are FORBIDDEN.
 *
 * Rules:
 * - payload MUST NOT be mutated
 * - ledger_seq MUST be strictly increasing
 * - payload_len excludes null terminator
 *
 * @traceability SRS-002-SHALL-002, SRS-002-SHALL-028
 */
typedef struct {
    ax_input_class_t  type;         /**< Input class */
    const uint8_t    *payload;      /**< AX:OBS:v1 canonical bytes */
    uint64_t          payload_len;  /**< Byte count (no null terminator) */
    uint64_t          ledger_seq;   /**< Ledger sequence for ordering */
} ax_input_t;

/* ========================================================================
 * AGENT CONTEXT (SRS-002-SHALL-024, SRS-002-SHALL-029)
 * ======================================================================== */

/**
 * @brief Agent context structure
 *
 * Complete state of the L5 behavioural agent. All fields are deterministic
 * and explicitly sized for cross-platform consistency.
 *
 * Rules:
 * - local_failed_flag is set if L6 commit fails (safety override)
 * - fault_accumulator resets ONLY on transition to INIT
 * - last_seen_seq enforces input ordering
 *
 * @traceability SRS-002-SHALL-024, SRS-002-SHALL-025, SRS-002-SHALL-028,
 *               SRS-002-SHALL-029
 */
typedef struct {
    ax_ledger_ctx_t   ledger;            /**< L6 audit substrate */
    ax_health_state_t health;            /**< Current behavioural state */
    uint32_t          fault_accumulator; /**< Deterministic fault counter */
    uint64_t          last_timestamp;    /**< Last admitted timestamp */
    uint64_t          last_seen_seq;     /**< Last processed ledger_seq */
    uint8_t           local_failed_flag; /**< Set if L6 commit fails */
    uint8_t           _pad[7];           /**< Padding — MUST be zeroed */
} ax_agent_ctx_t;

/* ========================================================================
 * FAULT THRESHOLDS (SRS-002-SHALL-024)
 * ======================================================================== */

/**
 * @defgroup FaultThresholds Fault Budget Thresholds
 *
 * Hardcoded constants for deterministic fault budget transitions.
 * MUST be identical across all builds.
 *
 * @{
 */

/** @brief Fault count threshold for ENABLED → ALARM transition */
#define AX_FAULT_THRESHOLD_ALARM  3U

/** @brief Fault count threshold for → STOPPED transition */
#define AX_FAULT_THRESHOLD_STOP   5U

/** @} */

/* ========================================================================
 * GOLDEN GENESIS REFERENCE (SRS-002-SHALL-026)
 * ======================================================================== */

/**
 * @brief Golden genesis hash for ledger binding verification
 *
 * Agent MUST verify ledger genesis_hash matches this reference
 * before transitioning UNINIT → INIT.
 *
 * Value: L0 from axioma-audit Phase 1 verification.
 *
 * @traceability SRS-002-SHALL-026
 */
extern const uint8_t AX_GOLDEN_GENESIS_HASH[32];

/* ========================================================================
 * TRANSITION EVIDENCE TAG
 * ======================================================================== */

/** @brief Evidence tag for state transitions */
#define AX_TAG_TRANS "AX:TRANS:v1"

/* ========================================================================
 * FUNCTION DECLARATIONS
 * ======================================================================== */

/**
 * @brief Initialize agent with ledger genesis
 *
 * Creates a new agent context and initializes the underlying L6 ledger.
 * This MUST be called before any other agent operations.
 *
 * POST-CONDITIONS:
 * - On success: ctx->health == AX_HEALTH_UNINIT
 * - On success: ctx->ledger.initialised == 1
 * - On failure: faults set, ctx zeroed
 *
 * @param ctx    Agent context to initialize (caller-owned)
 * @param faults Fault context for error propagation
 *
 * @traceability SRS-002-SHALL-026
 */
void ax_agent_init(
    ax_agent_ctx_t   *ctx,
    ct_fault_flags_t *faults
);

/**
 * @brief Bind agent to ledger (genesis verification)
 *
 * Verifies the ledger genesis_hash matches the golden reference
 * and transitions agent from UNINIT → INIT.
 *
 * GENESIS BINDING RULES (SRS-002-SHALL-026):
 * - Ledger MUST be initialised
 * - genesis_hash MUST match AX_GOLDEN_GENESIS_HASH
 * - Mismatch → GENESIS_MISMATCH violation → STOPPED
 *
 * @param ctx    Agent context
 * @param faults Fault context for error propagation
 *
 * @traceability SRS-002-SHALL-026
 */
void ax_agent_bind(
    ax_agent_ctx_t   *ctx,
    ct_fault_flags_t *faults
);

/**
 * @brief Process one input (primary transition engine)
 *
 * Implements the complete transition matrix from SRS-002 §9.1.
 * Processes exactly one input per call.
 *
 * EXECUTION ORDER (MANDATORY — SRS-002-SHALL-007):
 * 1. Validate input ordering (ledger_seq > last_seen_seq)
 * 2. Determine transition from matrix
 * 3. Construct AX:TRANS:v1 canonical payload
 * 4. Commit via axilog_commit() + ax_ledger_append()
 * 5. Mutate state ONLY if commit succeeds
 *
 * FAILURE HANDLING (SRS-002-SHALL-025):
 * If commit fails:
 *   ctx->health = AX_HEALTH_STOPPED
 *   ctx->local_failed_flag = 1
 *
 * @param ctx    Agent context
 * @param input  Admitted input to process
 * @param faults Fault context for error propagation
 *
 * @traceability SRS-002-SHALL-006, SRS-002-SHALL-007, SRS-002-SHALL-013,
 *               SRS-002-SHALL-014, SRS-002-SHALL-018, SRS-002-SHALL-025,
 *               SRS-002-SHALL-028
 */
void ax_agent_step(
    ax_agent_ctx_t   *ctx,
    const ax_input_t *input,
    ct_fault_flags_t *faults
);

/**
 * @brief Get health state as string
 *
 * Returns compile-time constant string for enum value.
 * Used for AX:TRANS:v1 serialization.
 *
 * @param state Health state enum value
 * @return Constant string or "UNKNOWN" for invalid values
 *
 * @traceability SRS-002-SHALL-027
 */
const char* ax_health_state_to_str(ax_health_state_t state);

/**
 * @brief Get input class as string
 *
 * Returns compile-time constant string for enum value.
 * Used for AX:TRANS:v1 serialization.
 *
 * @param input Input class enum value
 * @return Constant string or "UNKNOWN" for invalid values
 *
 * @traceability SRS-002-SHALL-027
 */
const char* ax_input_class_to_str(ax_input_class_t input);

/**
 * @brief Get violation type as string
 *
 * Returns compile-time constant string for enum value.
 * Used for AX:TRANS:v1 serialization.
 *
 * @param violation Violation enum value
 * @return Constant string or NULL for AX_VIOLATION_NONE
 *
 * @traceability SRS-002-SHALL-027
 */
const char* ax_violation_to_str(ax_violation_t violation);

/**
 * @brief Extract timestamp from TIME_OBS payload
 *
 * Parses the canonical JSON payload to extract the timestamp value.
 * Deterministic parsing with no ambiguity.
 *
 * EXPECTED FORMAT:
 *   {"timestamp":<uint64>}
 *
 * @param payload     Canonical JSON bytes
 * @param payload_len Byte count
 * @param out_ts      Output timestamp value
 * @param faults      Fault context for error propagation
 *
 * @return 1 on success, 0 on parse failure (faults->domain set)
 *
 * @traceability SRS-002-SHALL-009
 */
int ax_extract_timestamp(
    const uint8_t    *payload,
    uint64_t          payload_len,
    uint64_t         *out_ts,
    ct_fault_flags_t *faults
);

/**
 * @brief Constant-time hash comparison
 *
 * Compares two byte arrays without early exit to prevent timing attacks.
 * Uses XOR accumulation — NOT memcmp.
 *
 * @param a   First buffer
 * @param b   Second buffer
 * @param len Length to compare
 * @return 1 if equal, 0 if mismatch
 *
 * @traceability SRS-002-SHALL-026
 */
int ax_hash_equal_ct(const uint8_t *a, const uint8_t *b, size_t len);

#endif /* AXILOG_AGENT_H */
