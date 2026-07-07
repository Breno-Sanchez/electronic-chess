#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import os
import sys
import termios
import time
from pathlib import Path

try:
    import serial
except ImportError:
    print("Error: pyserial is required")
    print("Install with: python3 -m pip install pyserial")
    sys.exit(1)

LED_COUNT = 150
SQUARES = [f"{file}{rank}" for file in "abcdefgh" for rank in "12345678"]


def drain_serial(port: serial.Serial, seconds: float) -> None:
    end_time = time.monotonic() + seconds

    while time.monotonic() < end_time:
        waiting = port.in_waiting

        if waiting > 0:
            data = port.read(waiting)
            text = data.decode("utf-8", errors="ignore")

            for line in text.splitlines():
                if line.strip():
                    print(f"ESP: {line}")

        time.sleep(0.02)


def raw_write(port: serial.Serial, payload: bytes) -> None:
    fd = port.fileno()

    try:
        os.write(fd, payload)
        termios.tcdrain(fd)
    except OSError as exc:
        raise RuntimeError(f"Serial raw write failed: {exc}") from exc


def send_command(port: serial.Serial, command: str) -> None:
    payload = f"{command}\n".encode("ascii")

    try:
        port.write(payload)
        port.flush()
    except serial.SerialTimeoutException:
        print("Serial write timeout through pyserial, trying raw write fallback...")
        raw_write(port, payload)

    time.sleep(0.12)
    drain_serial(port, 0.20)


def send_led(port: serial.Serial, index: int) -> None:
    send_command(port, f"LED {index}")


def all_off(port: serial.Serial) -> None:
    send_command(port, "OFF")


def write_outputs(project_root: Path, mapping: dict[str, int]) -> None:
    json_path = project_root / "main" / "led_map.json"
    header_path = project_root / "main" / "led_map_generated.h"

    ordered = {sq: mapping[sq] for sq in SQUARES if sq in mapping}
    json_path.write_text(json.dumps(ordered, indent=2, sort_keys=False) + "\n", encoding="utf-8")

    rows: list[str] = []

    for rank in range(1, 9):
        values: list[str] = []

        for file in "abcdefgh":
            sq = f"{file}{rank}"

            if sq in mapping:
                values.append(f"{mapping[sq]:4d}U")
            else:
                values.append("0xFFFFU")

        rows.append("    { " + ", ".join(values) + " }")

    header = """#ifndef LED_MAP_GENERATED_H
#define LED_MAP_GENERATED_H

#include <stdint.h>

#define LED_MAP_INVALID_INDEX (0xFFFFU)

static const uint16_t ledMapGenerated[8][8] = {
""" + ",\n".join(rows) + """
};

#endif
"""

    header_path.write_text(header, encoding="utf-8")

    print()
    print(f"Saved JSON:   {json_path}")
    print(f"Saved header: {header_path}")


def wait_for_mapper(port: serial.Serial) -> None:
    print("Waiting for ESP32-S3 LED mapper firmware...")
    drain_serial(port, 4.0)
    all_off(port)
    print("Mapper link ready.")


def main() -> int:
    parser = argparse.ArgumentParser(description="Interactive LED-to-square mapper for Xadrez ESP32-S3")
    parser.add_argument("--port", default="/dev/ttyACM0")
    parser.add_argument("--baud", type=int, default=115200)
    args = parser.parse_args()

    project_root = Path(__file__).resolve().parents[1]
    mapping: dict[str, int] = {}

    print(f"Opening serial port {args.port} at {args.baud} baud...")

    with serial.Serial(
        args.port,
        args.baud,
        timeout=0.2,
        write_timeout=5.0,
        xonxoff=False,
        rtscts=False,
        dsrdtr=False,
    ) as port:
        port.reset_input_buffer()
        port.reset_output_buffer()

        port.dtr = True
        port.rts = False

        time.sleep(0.8)

        wait_for_mapper(port)

        led_index = 0

        for square in SQUARES:
            print()
            print("=" * 60)
            print(f"Mapping square: {square}")
            print("Place one magnet/piece on this square.")
            print("a = accept this LED for this square")
            print("r = reject and test next LED")
            print("b = go back one LED")
            print("q = quit and save partial map")
            print("=" * 60)

            while True:
                if led_index >= LED_COUNT:
                    print("Reached the last configured LED.")
                    write_outputs(project_root, mapping)
                    all_off(port)
                    return 1

                print(f"{square}: testing LED index {led_index}", flush=True)
                send_led(port, led_index)

                answer = input("[a/r/b/q] > ").strip().lower()

                if answer == "a":
                    mapping[square] = led_index
                    print(f"Accepted: {square} -> LED {led_index}")
                    led_index += 1
                    break

                if answer == "r":
                    led_index += 1
                    print(f"Rejected. Next LED index: {led_index}")
                    continue

                if answer == "b":
                    led_index = max(0, led_index - 1)
                    print(f"Back. LED index: {led_index}")
                    continue

                if answer == "q":
                    write_outputs(project_root, mapping)
                    all_off(port)
                    return 0

                print("Invalid input. Use a, r, b, or q.")

        write_outputs(project_root, mapping)
        all_off(port)

    print()
    print("Mapping complete.")
    print("Now build normal firmware with:")
    print("run chess")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
