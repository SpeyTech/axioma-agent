/**
 * @file agent.c
 * @brief Axilog Layer 5 Behavioural Agent Implementation
 *
 * DVEC: v1.3
 * DETERMINISM: D2 — Constrained Deterministic
 * MEMORY: Zero Dynamic Allocation
 *
 * Implements the total, bounded, deterministic state machine for
 * the Axioma framework. Every state transition is committed to
 * the L6 audit ledger prior to in-memory mutation.
 *
 * Copyright (c) 2026 The Murray Family Innovation Trust
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Patent: UK GB2521625.0
 *
 * @traceability SRS-002-SHALL-001 through SRS-002-SHALL-029
 */

#include <axilog/agent.h>
#include <axilog/commitment.h>
#include <string.h>
#include <stdio.h>

/* ========================================================================
 * GOLDEN GENESIS REFERENCE (SRS-002-SHALL-026)
 * ======================================================================== */

/**
 * @brief Golden genesis hash from Phase 1 verification
 *
 * Value: L0 = 7bb0d791697306ce2f1cc5df0bcdf66d810d6af9425aa380b352a62453a5ec7b
 *
 * @traceability SRS-002-SHALL-026
 */
const uint8_t AX_GOLDEN_GENESIS_HASH[32] = {
    0x7b, 0xb0, 0xd7, 0x91, 0x69, 0x73, 0x06, 0xce,
    0x2f, 0x1c, 0xc5, 0xdf, 0x0b, 0xcd, 0xf6, 0x6d,
    0x81, 0x0d, 0x6a, 0xf9, 0x42, 0x5a, 0xa3, 0x80,
    0xb3, 0x52, 0xa6, 0x24, 0x53, 0xa5, 0xec, 0x7b
};

/* ========================================================================
 * ENUM STRING MAPPING (SRS-002-SHALL-027)
 * ======================================================================== */

/**
 * @brief Health state to string mapping
 *
 * Compile-time constant lookup table.
 *
 * @traceability SRS-002-SHALL-027
 */
const char* ax_health_state_to_str(ax_health_state_t state)
{
    /* @traceability SRS-002-SHALL-027 */
    switch (state) {
        case AX_HEALTH_UNINIT:   return "UNINIT";
        case AX_HEALTH_INIT:     return "INIT";
        case AX_HEALTH_ENABLED:  return "ENABLED";
        case AX_HEALTH_ALARM:    return "ALARM";
        case AX_HEALTH_DEGRADED: return "DEGRADED";
        case AX_HEALTH_STOPPED:  return "STOPPED";
        default:                 return "UNKNOWN";
    }
}

/**
 * @brief Input class to string mapping
 *
 * Compile-time constant lookup table.
 *
 * @traceability SRS-002-SHALL-027
 */
const char* ax_input_class_to_str(ax_input_class_t input)
{
    /* @traceability SRS-002-SHALL-027 */
    switch (input) {
        case AX_INPUT_TIME_OBS:       return "TIME_OBS";
        case AX_INPUT_LLM_OBS:        return "LLM_OBS";
        case AX_INPUT_POLICY_TRIGGER: return "POLICY_TRIGGER";
        case AX_INPUT_FAULT_SIGNAL:   return "FAULT_SIGNAL";
        case AX_INPUT_RESET_REQUEST:  return "RESET_REQUEST";
        default:                      return "UNKNOWN";
    }
}

/**
 * @brief Violation type to string mapping
 *
 * Returns NULL for NONE (serialized as JSON null).
 *
 * @traceability SRS-002-SHALL-027
 */
const char* ax_violation_to_str(ax_violation_t violation)
{
    /* @traceability SRS-002-SHALL-027 */
    switch (violation) {
        case AX_VIOLATION_NONE:             return NULL;
        case AX_VIOLATION_TIME_ROLLBACK:    return "TIME_ROLLBACK";
        case AX_VIOLATION_POLICY_BREACH:    return "POLICY_BREACH";
        case AX_VIOLATION_FAULT_EXCEEDED:   return "FAULT_BUDGET_EXCEEDED";
        case AX_VIOLATION_PROTOCOL:         return "PROTOCOL_VIOLATION";
        case AX_VIOLATION_GENESIS_MISMATCH: return "GENESIS_MISMATCH";
        case AX_VIOLATION_COMMIT_FAILURE:   return "COMMIT_FAILURE";
        case AX_VIOLATION_SEQUENCE_ORDER:   return "SEQUENCE_ORDER";
        case AX_VIOLATION_TERMINALITY:      return "TERMINALITY";
        default:                            return "UNKNOWN";
    }
}

/* ========================================================================
 * CONSTANT-TIME COMPARISON (SRS-002-SHALL-026)
 * ======================================================================== */

/**
 * @brief Constant-time hash comparison
 *
 * Uses XOR accumulation to prevent timing attacks.
 * MUST NOT use memcmp (compiler may short-circuit).
 *
 * @traceability SRS-002-SHALL-026
 */
int ax_hash_equal_ct(const uint8_t *a, const uint8_t *b, size_t len)
{
    /* @traceability SRS-002-SHALL-026 */
    uint8_t diff = 0;
    size_t i;
    
    for (i = 0; i < len; i++) {
        diff |= a[i] ^ b[i];
    }
    
    return diff == 0;  /* 1 = equal, 0 = mismatch */
}

/* ========================================================================
 * TIMESTAMP EXTRACTION (SRS-002-SHALL-009)
 * ======================================================================== */

/**
 * @brief Extract timestamp from TIME_OBS payload
 *
 * Expects canonical JSON: {"timestamp":<uint64>}
 * Deterministic parsing — no ambiguity.
 *
 * @traceability SRS-002-SHALL-009
 */
int ax_extract_timestamp(
    const uint8_t    *payload,
    uint64_t          payload_len,
    uint64_t         *out_ts,
    ct_fault_flags_t *faults
)
{
    /* @traceability SRS-002-SHALL-009 */
    const char *prefix = "{\"timestamp\":";
    size_t prefix_len = 13;  /* strlen("{\"timestamp\":") */
    uint64_t ts = 0;
    size_t i;
    
    if (payload == NULL || out_ts == NULL || faults == NULL) {
        if (faults != NULL) faults->domain = 1;
        return 0;
    }
    
    /* Minimum valid: {"timestamp":0} = 14 chars */
    if (payload_len < 14) {
        faults->domain = 1;
        return 0;
    }
    
    /* Verify prefix */
    for (i = 0; i < prefix_len; i++) {
        if (payload[i] != (uint8_t)prefix[i]) {
            faults->domain = 1;
            return 0;
        }
    }
    
    /* Parse unsigned integer */
    i = prefix_len;
    while (i < payload_len && payload[i] >= '0' && payload[i] <= '9') {
        uint64_t digit = (uint64_t)(payload[i] - '0');
        
        /* Overflow check */
        if (ts > (UINT64_MAX - digit) / 10) {
            faults->domain = 1;
            return 0;
        }
        
        ts = ts * 10 + digit;
        i++;
    }
    
    /* Must have parsed at least one digit */
    if (i == prefix_len) {
        faults->domain = 1;
        return 0;
    }
    
    /* Verify closing brace */
    if (i >= payload_len || payload[i] != '}') {
        faults->domain = 1;
        return 0;
    }
    
    /* Verify exact length (no trailing garbage) */
    if (i + 1 != payload_len) {
        faults->domain = 1;
        return 0;
    }
    
    *out_ts = ts;
    return 1;
}

/* ========================================================================
 * AX:TRANS:v1 SERIALIZATION (SRS-002-SHALL-027)
 * ======================================================================== */

/** @brief Maximum AX:TRANS:v1 payload size */
#define AX_TRANS_MAX_SIZE 256

/**
 * @brief Serialize transition record to canonical JSON
 *
 * RFC 8785 (JCS) compliant: fields in lexicographic order.
 * Format: {"fault_count":N,"input_class":"X","ledger_seq":N,
 *          "next_state":"X","prev_state":"X","violation":null|"X"}
 *
 * @traceability SRS-002-SHALL-027
 */
static int ax_trans_serialize(
    char              *buf,
    size_t             size,
    ax_health_state_t  prev,
    ax_input_class_t   input,
    ax_health_state_t  next,
    ax_violation_t     violation,
    uint32_t           fault_count,
    uint64_t           ledger_seq
)
{
    /* @traceability SRS-002-SHALL-027 */
    int r;
    const char *viol_str = ax_violation_to_str(violation);
    
    /* Fields in lexicographic order: fault_count, input_class, ledger_seq,
     * next_state, prev_state, violation */
    if (viol_str == NULL) {
        r = snprintf(buf, size,
            "{\"fault_count\":%u,"
            "\"input_class\":\"%s\","
            "\"ledger_seq\":%llu,"
            "\"next_state\":\"%s\","
            "\"prev_state\":\"%s\","
            "\"violation\":null}",
            (unsigned)fault_count,
            ax_input_class_to_str(input),
            (unsigned long long)ledger_seq,
            ax_health_state_to_str(next),
            ax_health_state_to_str(prev)
        );
    } else {
        r = snprintf(buf, size,
            "{\"fault_count\":%u,"
            "\"input_class\":\"%s\","
            "\"ledger_seq\":%llu,"
            "\"next_state\":\"%s\","
            "\"prev_state\":\"%s\","
            "\"violation\":\"%s\"}",
            (unsigned)fault_count,
            ax_input_class_to_str(input),
            (unsigned long long)ledger_seq,
            ax_health_state_to_str(next),
            ax_health_state_to_str(prev),
            viol_str
        );
    }
    
    /* Validate snprintf result (SRS-002-SHALL-027) */
    if (r < 0 || (size_t)r >= size) {
        return -1;  /* Truncation or error */
    }
    
    return r;
}

/* ========================================================================
 * TRANSITION MATRIX (SRS-002-SHALL-013, SRS-002-SHALL-014)
 * ======================================================================== */

/**
 * @brief Transition result structure
 *
 * Output of transition matrix lookup.
 */
typedef struct {
    ax_health_state_t next_state;
    ax_violation_t    violation;
    uint8_t           is_noop;
} ax_transition_result_t;

/**
 * @brief Determine transition from current state and input
 *
 * Implements the complete transition matrix from SRS-002 §9.1.
 * Every (state, input) pair has exactly one outcome.
 *
 * @traceability SRS-002-SHALL-013, SRS-002-SHALL-014, SRS-002-SHALL-017
 */
static ax_transition_result_t ax_lookup_transition(
    ax_health_state_t state,
    ax_input_class_t  input,
    uint32_t          fault_count
)
{
    /* @traceability SRS-002-SHALL-013, SRS-002-SHALL-014 */
    ax_transition_result_t result;
    result.violation = AX_VIOLATION_NONE;
    result.is_noop = 0;
    
    switch (state) {
        case AX_HEALTH_UNINIT:
            /* @traceability SRS-002 §9.1 row 1-2 */
            if (input == AX_INPUT_RESET_REQUEST) {
                result.next_state = AX_HEALTH_INIT;
            } else {
                result.next_state = AX_HEALTH_STOPPED;
                result.violation = AX_VIOLATION_PROTOCOL;
            }
            break;
            
        case AX_HEALTH_INIT:
            /* @traceability SRS-002 §9.1 row 3-5 */
            if (input == AX_INPUT_TIME_OBS) {
                result.next_state = AX_HEALTH_ENABLED;
            } else if (input == AX_INPUT_FAULT_SIGNAL) {
                result.next_state = AX_HEALTH_STOPPED;
                result.violation = AX_VIOLATION_PROTOCOL;
            } else {
                /* No-Op */
                result.next_state = AX_HEALTH_INIT;
                result.is_noop = 1;
            }
            break;
            
        case AX_HEALTH_ENABLED:
            /* @traceability SRS-002 §9.1 row 6-10 */
            if (input == AX_INPUT_FAULT_SIGNAL) {
                /* Check fault threshold */
                if (fault_count + 1 >= AX_FAULT_THRESHOLD_STOP) {
                    result.next_state = AX_HEALTH_STOPPED;
                    result.violation = AX_VIOLATION_FAULT_EXCEEDED;
                } else if (fault_count + 1 >= AX_FAULT_THRESHOLD_ALARM) {
                    result.next_state = AX_HEALTH_ALARM;
                } else {
                    result.next_state = AX_HEALTH_ENABLED;
                }
            } else if (input == AX_INPUT_TIME_OBS ||
                       input == AX_INPUT_LLM_OBS ||
                       input == AX_INPUT_POLICY_TRIGGER) {
                result.next_state = AX_HEALTH_ENABLED;
            } else {
                /* No-Op for other inputs */
                result.next_state = AX_HEALTH_ENABLED;
                result.is_noop = 1;
            }
            break;
            
        case AX_HEALTH_ALARM:
            /* @traceability SRS-002 §9.1 row 11-13 */
            if (input == AX_INPUT_POLICY_TRIGGER) {
                result.next_state = AX_HEALTH_DEGRADED;
            } else if (input == AX_INPUT_FAULT_SIGNAL) {
                result.next_state = AX_HEALTH_STOPPED;
                result.violation = AX_VIOLATION_FAULT_EXCEEDED;
            } else {
                /* No-Op */
                result.next_state = AX_HEALTH_ALARM;
                result.is_noop = 1;
            }
            break;
            
        case AX_HEALTH_DEGRADED:
            /* @traceability SRS-002 §9.1 row 14-16 */
            if (input == AX_INPUT_RESET_REQUEST) {
                result.next_state = AX_HEALTH_INIT;
            } else if (input == AX_INPUT_FAULT_SIGNAL) {
                result.next_state = AX_HEALTH_STOPPED;
                result.violation = AX_VIOLATION_FAULT_EXCEEDED;
            } else {
                /* No-Op */
                result.next_state = AX_HEALTH_DEGRADED;
                result.is_noop = 1;
            }
            break;
            
        case AX_HEALTH_STOPPED:
            /* @traceability SRS-002 §9.1 row 17, SRS-002-SHALL-004 */
            result.next_state = AX_HEALTH_STOPPED;
            result.violation = AX_VIOLATION_TERMINALITY;
            break;
            
        default:
            /* @traceability SRS-002-SHALL-017 */
            result.next_state = AX_HEALTH_STOPPED;
            result.violation = AX_VIOLATION_PROTOCOL;
            break;
    }
    
    return result;
}

/* ========================================================================
 * AGENT INITIALIZATION (SRS-002-SHALL-026)
 * ======================================================================== */

/**
 * @brief Initialize agent with ledger genesis
 *
 * @traceability SRS-002-SHALL-026
 */
void ax_agent_init(
    ax_agent_ctx_t   *ctx,
    ct_fault_flags_t *faults
)
{
    /* @traceability SRS-002-SHALL-026 */
    
    if (ctx == NULL || faults == NULL) {
        if (faults != NULL) faults->domain = 1;
        return;
    }
    
    /* Zero entire context including padding */
    memset(ctx, 0, sizeof(ax_agent_ctx_t));
    
    /* Initialize L6 ledger */
    ax_ledger_genesis(&ctx->ledger, faults);
    
    if (ct_fault_any(faults)) {
        return;
    }
    
    /* Set initial state */
    ctx->health = AX_HEALTH_UNINIT;
    ctx->fault_accumulator = 0;
    ctx->last_timestamp = 0;
    ctx->last_seen_seq = 0;
    ctx->local_failed_flag = 0;
}

/**
 * @brief Bind agent to ledger (genesis verification)
 *
 * @traceability SRS-002-SHALL-026
 */
void ax_agent_bind(
    ax_agent_ctx_t   *ctx,
    ct_fault_flags_t *faults
)
{
    /* @traceability SRS-002-SHALL-026 */
    
    if (ctx == NULL || faults == NULL) {
        if (faults != NULL) faults->domain = 1;
        return;
    }
    
    /* Verify ledger is initialised */
    if (ctx->ledger.initialised != 1) {
        faults->domain = 1;
        ctx->health = AX_HEALTH_STOPPED;
        return;
    }
    
    /* Verify genesis hash matches golden reference */
    if (!ax_hash_equal_ct(ctx->ledger.genesis_hash, 
                          AX_GOLDEN_GENESIS_HASH, 32)) {
        /* Genesis mismatch → STOPPED */
        ctx->health = AX_HEALTH_STOPPED;
        faults->domain = 1;
        return;
    }
    
    /* Genesis verified — agent remains in UNINIT, ready for RESET_REQUEST */
}

/* ========================================================================
 * PRIMARY TRANSITION ENGINE (SRS-002-SHALL-006, SRS-002-SHALL-007)
 * ======================================================================== */

/**
 * @brief Process one input
 *
 * @traceability SRS-002-SHALL-006, SRS-002-SHALL-007, SRS-002-SHALL-013,
 *               SRS-002-SHALL-018, SRS-002-SHALL-025, SRS-002-SHALL-028
 */
void ax_agent_step(
    ax_agent_ctx_t   *ctx,
    const ax_input_t *input,
    ct_fault_flags_t *faults
)
{
    /* @traceability SRS-002-SHALL-006, SRS-002-SHALL-007 */
    
    ax_transition_result_t trans;
    ax_health_state_t prev_state;
    uint32_t fault_count_for_evidence;
    char trans_buf[AX_TRANS_MAX_SIZE];
    int ser_len;
    uint8_t commit[32];
    ct_fault_flags_t local_faults;
    
    /* === Input validation === */
    if (ctx == NULL || input == NULL || faults == NULL) {
        if (faults != NULL) faults->domain = 1;
        return;
    }
    
    /* Check local failure flag (SRS-002-SHALL-025) */
    if (ctx->local_failed_flag) {
        ctx->health = AX_HEALTH_STOPPED;
        faults->ledger_fail = 1;
        return;
    }
    
    /* Check ledger failure coupling (SRS-002-SHALL-005) */
    if (ctx->ledger.failed) {
        ctx->health = AX_HEALTH_STOPPED;
        ctx->local_failed_flag = 1;
        faults->ledger_fail = 1;
        return;
    }
    
    /* === Input ordering validation (SRS-002-SHALL-028) === */
    if (input->ledger_seq <= ctx->last_seen_seq && ctx->last_seen_seq > 0) {
        /* Sequence violation — but still record it */
        trans.next_state = AX_HEALTH_STOPPED;
        trans.violation = AX_VIOLATION_SEQUENCE_ORDER;
        trans.is_noop = 0;
        goto commit_transition;
    }
    
    /* === Time monotonicity check for TIME_OBS (SRS-002-SHALL-010) === */
    if (input->type == AX_INPUT_TIME_OBS) {
        uint64_t new_ts;
        ct_fault_init(&local_faults);
        
        if (!ax_extract_timestamp(input->payload, input->payload_len, 
                                   &new_ts, &local_faults)) {
            /* Parse failure → STOPPED */
            trans.next_state = AX_HEALTH_STOPPED;
            trans.violation = AX_VIOLATION_PROTOCOL;
            trans.is_noop = 0;
            goto commit_transition;
        }
        
        /* Monotonicity check (SRS-002-SHALL-010) */
        if (new_ts <= ctx->last_timestamp && ctx->last_timestamp > 0) {
            /* Time rollback → STOPPED (SRS-002-SHALL-011) */
            trans.next_state = AX_HEALTH_STOPPED;
            trans.violation = AX_VIOLATION_TIME_ROLLBACK;
            trans.is_noop = 0;
            goto commit_transition;
        }
    }
    
    /* === Determine transition (SRS-002-SHALL-013) === */
    trans = ax_lookup_transition(ctx->health, input->type, 
                                  ctx->fault_accumulator);

commit_transition:
    prev_state = ctx->health;
    
    /* Calculate fault count for evidence */
    if (input->type == AX_INPUT_FAULT_SIGNAL && 
        trans.violation != AX_VIOLATION_SEQUENCE_ORDER) {
        fault_count_for_evidence = ctx->fault_accumulator + 1;
    } else {
        fault_count_for_evidence = ctx->fault_accumulator;
    }
    
    /* Reset fault accumulator on transition to INIT (SRS-002-SHALL-029) */
    if (trans.next_state == AX_HEALTH_INIT && prev_state != AX_HEALTH_INIT) {
        fault_count_for_evidence = 0;
    }
    
    /* === Serialize AX:TRANS:v1 (SRS-002-SHALL-027) === */
    ser_len = ax_trans_serialize(
        trans_buf,
        sizeof(trans_buf),
        prev_state,
        input->type,
        trans.next_state,
        trans.violation,
        fault_count_for_evidence,
        input->ledger_seq
    );
    
    if (ser_len < 0) {
        /* Serialization failure → STOPPED + local flag */
        ctx->health = AX_HEALTH_STOPPED;
        ctx->local_failed_flag = 1;
        faults->domain = 1;
        return;
    }
    
    /* === Commit to ledger (SRS-002-SHALL-006) === */
    ct_fault_init(&local_faults);
    
    axilog_commit(
        AX_TAG_TRANS,
        (const uint8_t *)trans_buf,
        (uint64_t)ser_len,
        commit,
        &local_faults
    );
    
    if (ct_fault_any(&local_faults)) {
        /* Commit failure → STOPPED + local flag (SRS-002-SHALL-025) */
        ctx->health = AX_HEALTH_STOPPED;
        ctx->local_failed_flag = 1;
        faults->ledger_fail = 1;
        return;
    }
    
    /* Append to ledger chain */
    ax_ledger_append(&ctx->ledger, commit, &local_faults);
    
    if (ct_fault_any(&local_faults)) {
        /* Append failure → STOPPED + local flag (SRS-002-SHALL-025) */
        ctx->health = AX_HEALTH_STOPPED;
        ctx->local_failed_flag = 1;
        faults->ledger_fail = 1;
        return;
    }
    
    /* === Mutate state ONLY after successful commit (SRS-002-SHALL-007) === */
    ctx->health = trans.next_state;
    ctx->last_seen_seq = input->ledger_seq;
    
    /* Update fault accumulator */
    if (input->type == AX_INPUT_FAULT_SIGNAL && 
        trans.violation != AX_VIOLATION_SEQUENCE_ORDER) {
        ctx->fault_accumulator += 1;
    }
    
    /* Reset on transition to INIT (SRS-002-SHALL-029) */
    if (trans.next_state == AX_HEALTH_INIT && prev_state != AX_HEALTH_INIT) {
        ctx->fault_accumulator = 0;
    }
    
    /* Update timestamp for TIME_OBS */
    if (input->type == AX_INPUT_TIME_OBS && 
        trans.violation == AX_VIOLATION_NONE) {
        uint64_t new_ts;
        ct_fault_init(&local_faults);
        if (ax_extract_timestamp(input->payload, input->payload_len,
                                  &new_ts, &local_faults)) {
            ctx->last_timestamp = new_ts;
        }
    }
}
