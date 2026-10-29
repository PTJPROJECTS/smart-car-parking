/*
 * Parking lot logic: slot sensor debouncing and the entry gate state machine.
 * No Arduino code in here, so it can be unit tested on a PC.
 * All inputs are "1 = car present" (the IR modules are active low, the
 * firmware inverts them before calling this).
 */
#ifndef PARKING_H
#define PARKING_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PK_NUM_SLOTS      4
#define PK_DEBOUNCE_MS    150
#define PK_GATE_HOLD_MS   2000   /* gate stays open this long after car passes */
#define PK_GATE_OPEN_DEG  90
#define PK_GATE_SPEED     6      /* ms per degree, ~0.55 s full swing */

typedef struct {
    uint8_t stable;
    uint8_t last_raw;
    uint32_t changed_at;
} pk_debounce_t;

void pk_debounce_init(pk_debounce_t *d, uint8_t initial, uint32_t now);
/* returns 1 when the stable value changes */
int pk_debounce_update(pk_debounce_t *d, uint8_t raw, uint32_t now);

typedef enum {
    GATE_CLOSED = 0,
    GATE_OPENING,
    GATE_OPEN,
    GATE_CLOSING,
} pk_gate_state_t;

typedef struct {
    uint8_t slot[PK_NUM_SLOTS];
    uint8_t entry;
    uint8_t exit;
} pk_inputs_t;

typedef struct {
    pk_debounce_t slot[PK_NUM_SLOTS];
    pk_debounce_t entry;
    pk_debounce_t exit;
    pk_gate_state_t gate;
    uint32_t move_start;     /* when the current open/close move started */
    int start_angle;         /* gate angle at move_start */
    uint32_t clear_since;    /* when both gate sensors last went clear */
    uint8_t denied_latched;
} pk_lot_t;

/* event bits returned by pk_update() */
#define PK_EV_SLOTS   (1 << 0)
#define PK_EV_GATE    (1 << 1)
#define PK_EV_FULL    (1 << 2)   /* car at entry but no free slot */

void pk_init(pk_lot_t *lot, uint32_t now);
int pk_update(pk_lot_t *lot, const pk_inputs_t *in, uint32_t now);

int pk_free_slots(const pk_lot_t *lot);
int pk_gate_angle(const pk_lot_t *lot, uint32_t now);
const char *pk_gate_name(pk_gate_state_t s);

/* "ST slots=1010 free=2 gate=CLOSED" - this is what goes out on serial */
int pk_status_line(const pk_lot_t *lot, char *buf, size_t len);

#ifdef __cplusplus
}
#endif

#endif
