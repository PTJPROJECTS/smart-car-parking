#!/usr/bin/env python3
"""Random traffic generator for the parking simulator.

Makes cars arrive and leave at random, feeds the input lines to
build/parking_sim and prints what the "Uno" would print on serial.

    python3 tools/sim_cars.py --cars 6
    python3 tools/sim_cars.py --realtime     # for the dashboard
"""
import argparse
import os
import random
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SIM = os.path.join(ROOT, "build", "parking_sim")
NUM_SLOTS = 4


def traffic(n_cars, seed):
    """Yield (t_ms, slots, entry, exit) input lines."""
    rng = random.Random(seed)
    slots = [0] * NUM_SLOTS
    t = 0
    yield (t, slots[:], 0, 0)

    parked = []
    for _ in range(n_cars):
        t += rng.randint(3000, 8000)
        # sometimes a car leaves first
        if parked and rng.random() < 0.4:
            s = parked.pop(rng.randrange(len(parked)))
            slots[s] = 0
            yield (t, slots[:], 0, 0)
            t += 1500
            yield (t, slots[:], 0, 1)
            t += 1200
            yield (t, slots[:], 0, 0)
            t += rng.randint(2000, 4000)

        yield (t, slots[:], 1, 0)
        free = [i for i in range(NUM_SLOTS) if not slots[i]]
        if not free:
            # driver gives up and reverses out
            t += 4000
            yield (t, slots[:], 0, 0)
            continue
        t += 1400
        yield (t, slots[:], 0, 0)
        t += rng.randint(3000, 6000)
        s = rng.choice(free)
        slots[s] = 1
        parked.append(s)
        yield (t, slots[:], 0, 0)

    yield (t + 5000, slots[:], 0, 0)


def fmt(ev):
    t, slots, entry, ex = ev
    return "%d %s %d %d\n" % (t, "".join(str(s) for s in slots), entry, ex)


def run(n_cars, seed, realtime, out=sys.stdout):
    if not os.path.exists(SIM):
        sys.exit("build/parking_sim missing, run make first")
    proc = subprocess.Popen([SIM], stdin=subprocess.PIPE,
                            stdout=out, text=True, bufsize=1)
    last_t = 0
    for ev in traffic(n_cars, seed):
        if realtime:
            time.sleep((ev[0] - last_t) / 1000.0)
            last_t = ev[0]
        proc.stdin.write(fmt(ev))
        proc.stdin.flush()
    proc.stdin.close()
    proc.wait()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cars", type=int, default=10)
    ap.add_argument("--seed", type=int, default=3)
    ap.add_argument("--realtime", action="store_true")
    ap.add_argument("--dump", action="store_true",
                    help="print the input lines instead of running the sim")
    args = ap.parse_args()

    if args.dump:
        for ev in traffic(args.cars, args.seed):
            sys.stdout.write(fmt(ev))
        return
    run(args.cars, args.seed, args.realtime)


if __name__ == "__main__":
    main()
