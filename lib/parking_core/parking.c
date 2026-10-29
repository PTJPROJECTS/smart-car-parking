#include "parking.h"

#include <stdio.h>

void pk_debounce_init(pk_debounce_t *d, uint8_t initial, uint32_t now)
{
    d->stable = initial;
    d->last_raw = initial;
    d->changed_at = now;
}

int pk_debounce_update(pk_debounce_t *d, uint8_t raw, uint32_t now)
{
    if (raw != d->last_raw) {
        d->last_raw = raw;
        d->changed_at = now;
        return 0;
    }
    /* unsigned subtraction handles millis() wrap */
    if (raw != d->stable && (uint32_t)(now - d->changed_at) >= PK_DEBOUNCE_MS) {
        d->stable = raw;
        return 1;
    }
    return 0;
}

void pk_init(pk_lot_t *lot, uint32_t now)
{
    int i;

    for (i = 0; i < PK_NUM_SLOTS; i++)
        pk_debounce_init(&lot->slot[i], 0, now);
    pk_debounce_init(&lot->entry, 0, now);
    pk_debounce_init(&lot->exit, 0, now);
    lot->gate = GATE_CLOSED;
    lot->move_start = now;
    lot->start_angle = 0;
    lot->clear_since = now;
    lot->denied_latched = 0;
}

int pk_free_slots(const pk_lot_t *lot)
{
    int i, n = 0;

    for (i = 0; i < PK_NUM_SLOTS; i++)
        if (!lot->slot[i].stable)
            n++;
    return n;
}

int pk_gate_angle(const pk_lot_t *lot, uint32_t now)
{
    int moved = (int)((uint32_t)(now - lot->move_start) / PK_GATE_SPEED);
    int a;

    switch (lot->gate) {
    case GATE_OPEN:
        return PK_GATE_OPEN_DEG;
    case GATE_OPENING:
        a = lot->start_angle + moved;
        return a > PK_GATE_OPEN_DEG ? PK_GATE_OPEN_DEG : a;
    case GATE_CLOSING:
        a = lot->start_angle - moved;
        return a < 0 ? 0 : a;
    case GATE_CLOSED:
    default:
        return 0;
    }
}

static void gate_move(pk_lot_t *lot, pk_gate_state_t to, uint32_t now)
{
    lot->start_angle = pk_gate_angle(lot, now);
    lot->move_start = now;
    lot->gate = to;
}

int pk_update(pk_lot_t *lot, const pk_inputs_t *in, uint32_t now)
{
    int ev = 0;
    int i;
    int car_at_gate;
    pk_gate_state_t before = lot->gate;

    for (i = 0; i < PK_NUM_SLOTS; i++)
        if (pk_debounce_update(&lot->slot[i], in->slot[i], now))
            ev |= PK_EV_SLOTS;
    pk_debounce_update(&lot->entry, in->entry, now);
    pk_debounce_update(&lot->exit, in->exit, now);

    car_at_gate = lot->entry.stable || lot->exit.stable;
    if (car_at_gate)
        lot->clear_since = now;
    if (!lot->entry.stable)
        lot->denied_latched = 0;

    switch (lot->gate) {
    case GATE_CLOSED:
        if (lot->exit.stable) {
            gate_move(lot, GATE_OPENING, now);
        } else if (lot->entry.stable) {
            if (pk_free_slots(lot) > 0) {
                gate_move(lot, GATE_OPENING, now);
            } else if (!lot->denied_latched) {
                lot->denied_latched = 1;
                ev |= PK_EV_FULL;
            }
        }
        break;

    case GATE_OPENING:
        if (pk_gate_angle(lot, now) >= PK_GATE_OPEN_DEG) {
            lot->gate = GATE_OPEN;
            lot->clear_since = now;
        }
        break;

    case GATE_OPEN:
        if ((uint32_t)(now - lot->clear_since) >= PK_GATE_HOLD_MS)
            gate_move(lot, GATE_CLOSING, now);
        break;

    case GATE_CLOSING:
        /* Something under the boom, go back up. Uses the raw inputs on
         * purpose: waiting 150 ms for the debounce let the boom drop
         * another ~25 degrees. A false reopen is cheap, hitting a car isn't. */
        if (car_at_gate || in->entry || in->exit)
            gate_move(lot, GATE_OPENING, now);
        else if (pk_gate_angle(lot, now) <= 0)
            lot->gate = GATE_CLOSED;
        break;
    }

    if (lot->gate != before)
        ev |= PK_EV_GATE;
    return ev;
}

const char *pk_gate_name(pk_gate_state_t s)
{
    switch (s) {
    case GATE_CLOSED:  return "CLOSED";
    case GATE_OPENING: return "OPENING";
    case GATE_OPEN:    return "OPEN";
    case GATE_CLOSING: return "CLOSING";
    }
    return "?";
}

int pk_status_line(const pk_lot_t *lot, char *buf, size_t len)
{
    char slots[PK_NUM_SLOTS + 1];
    int i, n;

    for (i = 0; i < PK_NUM_SLOTS; i++)
        slots[i] = lot->slot[i].stable ? '1' : '0';
    slots[PK_NUM_SLOTS] = '\0';

    n = snprintf(buf, len, "ST slots=%s free=%d gate=%s", slots,
                 pk_free_slots(lot), pk_gate_name(lot->gate));
    if (n < 0 || (size_t)n >= len)
        return -1;
    return n;
}
