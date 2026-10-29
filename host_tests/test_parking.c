#include <stdio.h>
#include <string.h>

#include "parking.h"

static int checks, failures;

#define CHECK(cond)                                                   \
    do {                                                              \
        checks++;                                                     \
        if (!(cond)) {                                                \
            failures++;                                               \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);    \
        }                                                             \
    } while (0)

/* run the lot for `ms` milliseconds with fixed inputs, 10 ms loop like the
 * firmware. Returns OR of all events seen. */
static int run(pk_lot_t *lot, const pk_inputs_t *in, uint32_t *now, uint32_t ms)
{
    int ev = 0;
    uint32_t end = *now + ms;

    while (*now < end) {
        *now += 10;
        ev |= pk_update(lot, in, *now);
    }
    return ev;
}

static void test_debounce_ignores_glitch(void)
{
    pk_debounce_t d;
    uint32_t t = 0;
    int changed = 0;

    pk_debounce_init(&d, 0, 0);
    /* 100 ms of chatter then back to 0 */
    for (t = 10; t <= 100; t += 10)
        changed |= pk_debounce_update(&d, (t / 10) & 1, t);
    for (; t <= 400; t += 10)
        changed |= pk_debounce_update(&d, 0, t);

    CHECK(changed == 0);
    CHECK(d.stable == 0);
}

static void test_debounce_accepts_steady(void)
{
    pk_debounce_t d;
    uint32_t t;
    uint32_t changed_at = 0;

    pk_debounce_init(&d, 0, 0);
    for (t = 10; t <= 400; t += 10)
        if (pk_debounce_update(&d, 1, t) && !changed_at)
            changed_at = t;

    CHECK(d.stable == 1);
    CHECK(changed_at >= 10 + PK_DEBOUNCE_MS);
    CHECK(changed_at <= 20 + PK_DEBOUNCE_MS);
}

static void test_debounce_millis_wrap(void)
{
    pk_debounce_t d;
    uint32_t t = 0xFFFFFF00u;
    int i, changed = 0;

    pk_debounce_init(&d, 0, t);
    for (i = 0; i < 40; i++) {
        t += 10;   /* wraps past zero */
        changed |= pk_debounce_update(&d, 1, t);
    }
    CHECK(changed);
    CHECK(d.stable == 1);
}

static void test_free_slot_count(void)
{
    pk_lot_t lot;
    pk_inputs_t in = { {1, 0, 1, 0}, 0, 0 };
    uint32_t now = 0;
    char line[64];

    pk_init(&lot, 0);
    CHECK(pk_free_slots(&lot) == 4);
    CHECK(run(&lot, &in, &now, 300) & PK_EV_SLOTS);
    CHECK(pk_free_slots(&lot) == 2);

    CHECK(pk_status_line(&lot, line, sizeof(line)) > 0);
    CHECK(strcmp(line, "ST slots=1010 free=2 gate=CLOSED") == 0);
}

static void test_gate_cycle_for_entry(void)
{
    pk_lot_t lot;
    pk_inputs_t in;
    uint32_t now = 0;

    memset(&in, 0, sizeof(in));
    pk_init(&lot, 0);

    in.entry = 1;
    run(&lot, &in, &now, 200);
    CHECK(lot.gate == GATE_OPENING);

    run(&lot, &in, &now, 1000);
    CHECK(lot.gate == GATE_OPEN);
    CHECK(pk_gate_angle(&lot, now) == PK_GATE_OPEN_DEG);

    /* car still under the gate, must stay open */
    run(&lot, &in, &now, 5000);
    CHECK(lot.gate == GATE_OPEN);

    in.entry = 0;
    run(&lot, &in, &now, 1000);
    CHECK(lot.gate == GATE_OPEN);       /* hold time not done yet */
    run(&lot, &in, &now, 1500);
    CHECK(lot.gate == GATE_CLOSING);
    run(&lot, &in, &now, 1000);
    CHECK(lot.gate == GATE_CLOSED);
    CHECK(pk_gate_angle(&lot, now) == 0);
}

static void test_full_lot_denies_entry(void)
{
    pk_lot_t lot;
    pk_inputs_t in = { {1, 1, 1, 1}, 0, 0 };
    uint32_t now = 0;
    int ev;

    pk_init(&lot, 0);
    run(&lot, &in, &now, 300);
    CHECK(pk_free_slots(&lot) == 0);

    in.entry = 1;
    ev = run(&lot, &in, &now, 2000);
    CHECK(ev & PK_EV_FULL);
    CHECK(lot.gate == GATE_CLOSED);

    /* FULL is reported once per car, not every loop */
    ev = run(&lot, &in, &now, 2000);
    CHECK(!(ev & PK_EV_FULL));
}

static void test_exit_opens_even_when_full(void)
{
    pk_lot_t lot;
    pk_inputs_t in = { {1, 1, 1, 1}, 0, 1 };
    uint32_t now = 0;

    pk_init(&lot, 0);
    run(&lot, &in, &now, 300);
    CHECK(lot.gate == GATE_OPENING || lot.gate == GATE_OPEN);
}

static void test_reopen_while_closing(void)
{
    pk_lot_t lot;
    pk_inputs_t in;
    uint32_t now = 0;
    int angle;

    memset(&in, 0, sizeof(in));
    pk_init(&lot, 0);
    in.entry = 1;
    run(&lot, &in, &now, 1000);
    in.entry = 0;
    run(&lot, &in, &now, 2300);
    CHECK(lot.gate == GATE_CLOSING);
    angle = pk_gate_angle(&lot, now);
    CHECK(angle > 0 && angle < PK_GATE_OPEN_DEG);

    /* car rolls back in under the boom */
    in.entry = 1;
    run(&lot, &in, &now, 160);
    CHECK(lot.gate == GATE_OPENING);
    /* should go up from where it was, not jump to 0 */
    CHECK(pk_gate_angle(&lot, now) >= angle - 3);
}

int main(void)
{
    test_debounce_ignores_glitch();
    test_debounce_accepts_steady();
    test_debounce_millis_wrap();
    test_free_slot_count();
    test_gate_cycle_for_entry();
    test_full_lot_denies_entry();
    test_exit_opens_even_when_full();
    test_reopen_while_closing();

    printf("%d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}
