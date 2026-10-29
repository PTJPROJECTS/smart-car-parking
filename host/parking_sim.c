/*
 * Runs the real parking logic on a PC.
 *
 * Input, one line per change (time in ms, must not go backwards):
 *     <t_ms> <slots> <entry> <exit>
 *     1000 1010 1 0
 * Lines starting with # are ignored. The sim steps the logic every 10 ms
 * like the firmware loop and prints the same lines the Uno prints on serial.
 */
#include <stdio.h>
#include <string.h>

#include "parking.h"

#define STEP_MS 10

static void emit(const pk_lot_t *lot, int ev, unsigned long t, int stamp)
{
    char line[64];

    if (!ev)
        return;
    if (stamp)
        printf("[%7lu] ", t);
    if (ev & PK_EV_FULL) {
        printf("EV full\n");
        if (stamp)
            printf("[%7lu] ", t);
    }
    pk_status_line(lot, line, sizeof(line));
    printf("%s\n", line);
    fflush(stdout);
}

int main(int argc, char **argv)
{
    pk_lot_t lot;
    pk_inputs_t in;
    unsigned long now = 0;
    char buf[128];
    int stamp = argc > 1 && strcmp(argv[1], "-t") == 0;

    memset(&in, 0, sizeof(in));
    pk_init(&lot, 0);
    emit(&lot, PK_EV_SLOTS, 0, stamp);

    while (fgets(buf, sizeof(buf), stdin)) {
        unsigned long t;
        char slots[16];
        int entry, ex, i;

        if (buf[0] == '#' || buf[0] == '\n')
            continue;
        if (sscanf(buf, "%lu %15s %d %d", &t, slots, &entry, &ex) != 4 ||
            strlen(slots) != PK_NUM_SLOTS) {
            fprintf(stderr, "bad line: %s", buf);
            return 1;
        }

        /* run with the old inputs up to time t */
        while (now + STEP_MS <= t) {
            now += STEP_MS;
            emit(&lot, pk_update(&lot, &in, (uint32_t)now), now, stamp);
        }

        for (i = 0; i < PK_NUM_SLOTS; i++)
            in.slot[i] = slots[i] == '1';
        in.entry = (uint8_t)entry;
        in.exit = (uint8_t)ex;
    }
    return 0;
}
