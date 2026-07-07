#!/usr/bin/env python3
from __future__ import annotations

import json
import re
import sys
from pathlib import Path

SQUARES = [f"{file}{rank}" for file in "abcdefgh" for rank in "12345678"]
ANSI_RE = re.compile(r"\x1b\[[0-9;?]*[A-Za-z]")
ACCEPT_RE = re.compile(r"MAP_ACCEPT\s+([a-h][1-8])\s+([0-9]+)")


def clean_text(text: str) -> str:
    text = ANSI_RE.sub("", text)
    text = text.replace("\r", "\n")
    return text


def main() -> int:
    if len(sys.argv) != 2:
        print("Usage: parse_led_map.py <monitor-log>")
        return 2

    project_root = Path(__file__).resolve().parents[1]
    log_path = Path(sys.argv[1])

    text = clean_text(log_path.read_text(encoding="utf-8", errors="ignore"))

    mapping: dict[str, int] = {}

    for match in ACCEPT_RE.finditer(text):
        square = match.group(1)
        led_index = int(match.group(2))
        mapping[square] = led_index

    if not mapping:
        print("No MAP_ACCEPT lines found.")
        print(f"Log kept at: {log_path}")
        return 1

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

    print(f"Accepted squares: {len(mapping)}")
    print(f"Saved JSON:       {json_path}")
    print(f"Saved header:     {header_path}")
    print(f"Monitor log:      {log_path}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
