#!/usr/bin/env python3
from __future__ import annotations

import re
import shutil
import subprocess
from pathlib import Path

ROOT = Path.cwd()
HERE = Path(__file__).resolve().parent
FILES = HERE / "files"


def run(cmd: list[str]) -> None:
    try:
        subprocess.run(cmd, check=False)
    except FileNotFoundError:
        pass


def extract_define(text: str, name: str, fallback: str) -> str:
    match = re.search(rf'^#define\s+{re.escape(name)}\s+"([^"]*)"', text, re.MULTILINE)
    if match is None:
        return fallback
    return match.group(1)


def patch_main_c() -> None:
    path = ROOT / "main" / "main.c"
    text = path.read_text(encoding="utf-8")
    text = re.sub(r'#define\s+SENSOR_QUEUE_LEN\s+\(.*?\)', '#define SENSOR_QUEUE_LEN            (96U)', text)
    text = re.sub(r'#define\s+LED_QUEUE_LEN\s+\(.*?\)', '#define LED_QUEUE_LEN               (16U)', text)
    path.write_text(text, encoding="utf-8")


def copy_file(relative: str, content: str | None = None) -> None:
    dst = ROOT / relative
    dst.parent.mkdir(parents=True, exist_ok=True)
    if content is None:
        shutil.copy2(FILES / relative, dst)
    else:
        dst.write_text(content, encoding="utf-8")


def main() -> int:
    if not (ROOT / "main").is_dir():
        print("Error: run this script from the project root, e.g. ~/Downloads/OI/Xadrez")
        return 2

    server_old_path = ROOT / "main" / "server.c"
    old_server = server_old_path.read_text(encoding="utf-8", errors="ignore") if server_old_path.exists() else ""

    wifi_ssid = extract_define(old_server, "WIFI_SSID_TXT", "UTFPR-ALUNO")
    eap_identity = extract_define(old_server, "EAP_IDENTITY_TXT", "CHANGE_ME")
    eap_username = extract_define(old_server, "EAP_USERNAME_TXT", "CHANGE_ME")
    eap_password = extract_define(old_server, "EAP_PASSWORD_TXT", "CHANGE_ME")

    run(["git", "rm", "--ignore-unmatch", "bestValid", "legalCount", "physical[rank]", "sequence"])

    copy_file("main/app_types.h")
    copy_file("main/chess_logic.h")
    copy_file("main/chess_engine.h")
    copy_file("main/chess_logic.c")
    copy_file("main/chess_engine.c")
    copy_file("main/led.c")
    copy_file("main/CMakeLists.txt")

    # Keep the calibrated map if it already exists. Install a fallback only for fresh clones.
    if not (ROOT / "main" / "led_map_generated.h").exists():
        copy_file("main/led_map_generated.h")

    server = (FILES / "main" / "server.c").read_text(encoding="utf-8")
    server = server.replace("__WIFI_SSID__", wifi_ssid)
    server = server.replace("__EAP_IDENTITY__", eap_identity)
    server = server.replace("__EAP_USERNAME__", eap_username)
    server = server.replace("__EAP_PASSWORD__", eap_password)
    copy_file("main/server.c", server)

    patch_main_c()

    print("Applied Xadrez game-state refactor patch.")
    print("Next: run chess build")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
