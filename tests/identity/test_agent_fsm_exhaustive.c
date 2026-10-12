/**
 * @file test_agent_fsm_exhaustive.c
 * @brief Exhaustive (State x Input) drive of the real transition function
 *
 * DVEC: v1.3
 * DETERMINISM: D2 — Constrained Deterministic
 *
 * test_agent_totality.c::test_totality_coverage checks only that every
 * state and input enum has a string mapping. This test drives
 * ax_agent_step() itself from every health state with every declared
 * input class and with out-of-range input values, and asserts:
 *
 *   - the resulting state is a declared ax_health_state_t value;
 *   - STOPPED is absorbing for every input;
 *   - every step extends the ledger by exactly one record.
 *
 * It asserts only behaviour observed at the time it was written; it does
 * not encode the open SRS-002 deviations (fault accumulator mutation in
 * STOPPED, reset semantics) as correct or incorrect.
 *
 * Copyright (c) 2026 Spey Systems LTD
 * SPDX-License-Identifier: AGPL-3.0-or-later
 * Patent: UK GB2521625.0
 *
 * @traceability SRS-002-SHALL-013, SRS-002-SHALL-014
 */

#include <axilog/agent.h>
#include <axilog/types.h>
#include <stdio.h>
#include <string.h>

static const uint8_t TS_PAYLOAD[] = "{\"timestamp\":100}";

/* Declared inputs 0..4, then values outside the enum. */
static const int INPUTS[] = { 0, 1, 2, 3, 4, 5, 99, -1 };
#define N_INPUTS (sizeof(INPUTS) / sizeof(INPUTS[0]))

static int fresh_bound(ax_agent_ctx_t *ctx)
{
    ct_fault_flags_t faults;

    ct_fault_init(&faults);
    ax_agent_init(ctx, &faults);
    ax_agent_bind(ctx, &faults);
    return ct_fault_any(&faults);
}

int main(void)
{
    int state;
    size_t i;
    int pairs = 0;
    int failures = 0;

    for (state = (int)AX_HEALTH_UNINIT; state < AX_HEALTH_STATE_COUNT; state++) {
        for (i = 0U; i < N_INPUTS; i++) {
            ax_agent_ctx_t ctx;
            ax_input_t in;
            ct_fault_flags_t faults;
            uint64_t seq0;

            if (fresh_bound(&ctx) != 0) {
                printf("FAIL: init/bind raised a fault\n");
                return 1;
            }
            ctx.health = (ax_health_state_t)state;
            seq0 = ctx.ledger.sequence;

            memset(&in, 0, sizeof(in));
            in.type = (ax_input_class_t)INPUTS[i];
            in.ledger_seq = 1U;
            in.payload = TS_PAYLOAD;
            in.payload_len = sizeof(TS_PAYLOAD) - 1U;

            ct_fault_init(&faults);
            ax_agent_step(&ctx, &in, &faults);
            pairs++;

            if ((int)ctx.health < (int)AX_HEALTH_UNINIT ||
                (int)ctx.health >= AX_HEALTH_STATE_COUNT) {
                printf("FAIL: state %d input %d -> undeclared state %d\n",
                       state, INPUTS[i], (int)ctx.health);
                failures++;
            }
            if (state == (int)AX_HEALTH_STOPPED &&
                ctx.health != AX_HEALTH_STOPPED) {
                printf("FAIL: STOPPED left on input %d\n", INPUTS[i]);
                failures++;
            }
            if (ctx.ledger.sequence != seq0 + 1U) {
                printf("FAIL: state %d input %d ledger delta %llu\n",
                       state, INPUTS[i],
                       (unsigned long long)(ctx.ledger.sequence - seq0));
                failures++;
            }
        }
    }

    printf("%d (state x input) pairs driven, %d failures\n", pairs, failures);
    return failures == 0 ? 0 : 1;
}
