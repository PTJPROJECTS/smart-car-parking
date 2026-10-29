#!/usr/bin/env python3
"""Small Flask page that shows the parking lot status.

Reads the status lines from the Uno over serial, or from the simulator:

    python3 dashboard/app.py --sim
    python3 dashboard/app.py --serial /dev/ttyUSB0

then open http://localhost:5000
"""
import argparse
import collections
import os
import re
import subprocess
import sys
import threading
import time

from flask import Flask, jsonify, render_template

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
STATUS_RE = re.compile(r"^ST slots=([01]+) free=(\d+) gate=([A-Z]+)$")


def parse_line(line):
    """Turn one serial line into a dict. Returns None for junk."""
    line = line.strip()
    m = STATUS_RE.match(line)
    if m:
        return {
            "type": "status",
            "slots": [c == "1" for c in m.group(1)],
            "free": int(m.group(2)),
            "gate": m.group(3),
        }
    if line == "EV full":
        return {"type": "full"}
    return None


class LotState:
    def __init__(self):
        self.lock = threading.Lock()
        self.slots = []
        self.free = None
        self.gate = "UNKNOWN"
        self.full_count = 0
        self.updated = None
        self.log = collections.deque(maxlen=15)

    def feed(self, line):
        msg = parse_line(line)
        if msg is None:
            return False
        with self.lock:
            if msg["type"] == "status":
                self.slots = msg["slots"]
                self.free = msg["free"]
                self.gate = msg["gate"]
            else:
                self.full_count += 1
            self.updated = time.time()
            self.log.appendleft("%s  %s" % (time.strftime("%H:%M:%S"),
                                            line.strip()))
        return True

    def snapshot(self):
        with self.lock:
            return {
                "slots": list(self.slots),
                "free": self.free,
                "gate": self.gate,
                "full_count": self.full_count,
                "updated": self.updated,
                "log": list(self.log),
            }


def create_app(state):
    app = Flask(__name__)

    @app.route("/")
    def index():
        return render_template("index.html", s=state.snapshot())

    @app.route("/api/status")
    def status():
        return jsonify(state.snapshot())

    return app


def read_serial(state, port, baud):
    import serial  # pyserial, only needed with real hardware

    while True:
        try:
            with serial.Serial(port, baud, timeout=1) as ser:
                while True:
                    raw = ser.readline()
                    if raw:
                        state.feed(raw.decode("ascii", "replace"))
        except serial.SerialException as e:
            # USB cable gets bumped, just retry
            print("serial error: %s, retrying" % e, file=sys.stderr)
            time.sleep(2)


def read_sim(state):
    while True:
        proc = subprocess.Popen(
            [sys.executable, os.path.join(ROOT, "tools", "sim_cars.py"),
             "--realtime", "--cars", "30", "--seed", str(int(time.time()))],
            stdout=subprocess.PIPE, text=True)
        for line in proc.stdout:
            state.feed(line)
        proc.wait()


def main():
    ap = argparse.ArgumentParser()
    src = ap.add_mutually_exclusive_group(required=True)
    src.add_argument("--serial", metavar="PORT")
    src.add_argument("--sim", action="store_true")
    ap.add_argument("--baud", type=int, default=9600)
    ap.add_argument("--port", type=int, default=5000)
    args = ap.parse_args()

    state = LotState()
    if args.sim:
        t = threading.Thread(target=read_sim, args=(state,), daemon=True)
    else:
        t = threading.Thread(target=read_serial,
                             args=(state, args.serial, args.baud), daemon=True)
    t.start()

    create_app(state).run(host="0.0.0.0", port=args.port)


if __name__ == "__main__":
    main()
