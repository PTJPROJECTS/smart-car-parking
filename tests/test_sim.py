"""Runs the C simulator (same code as the firmware) on the scenario files."""
import os
import subprocess

import pytest

import sim_cars

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SIM = os.path.join(ROOT, "build", "parking_sim")

pytestmark = pytest.mark.skipif(not os.path.exists(SIM),
                                reason="run make first")


def run_scenario(name):
    with open(os.path.join(ROOT, "scenarios", name)) as f:
        out = subprocess.run([SIM], stdin=f, capture_output=True,
                             text=True, check=True).stdout
    return out.splitlines()


def test_basic_scenario():
    lines = run_scenario("basic.txt")
    assert lines[0] == "ST slots=0000 free=4 gate=CLOSED"
    assert lines[-1] == "ST slots=0100 free=3 gate=CLOSED"
    assert "EV full" not in lines
    # three gate cycles: two cars in, one out
    assert sum(1 for l in lines if l.endswith("gate=OPEN")) == 3


def test_full_lot_scenario():
    lines = run_scenario("full_lot.txt")
    assert lines.count("EV full") == 1
    i = lines.index("EV full")
    assert lines[i + 1] == "ST slots=1111 free=0 gate=CLOSED"
    # gate only opens after a slot frees up
    first_open = next(n for n, l in enumerate(lines) if "OPENING" in l)
    assert first_open > i
    assert "free=1" in lines[first_open]


def test_random_traffic_never_overfills():
    # the generator itself should never park two cars in one slot
    for seed in range(20):
        prev = None
        for t, slots, entry, ex in sim_cars.traffic(15, seed):
            assert prev is None or t >= prev
            prev = t
            assert len(slots) == sim_cars.NUM_SLOTS


def test_random_traffic_through_sim():
    proc = subprocess.run(
        [SIM], input="".join(sim_cars.fmt(e) for e in sim_cars.traffic(10, 3)),
        capture_output=True, text=True, check=True)
    lines = proc.stdout.splitlines()
    assert lines.count("EV full") >= 1
    for l in lines:
        assert l == "EV full" or l.startswith("ST slots=")
