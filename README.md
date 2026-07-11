# Xadrez Eletrônico ESP32-S3

Firmware for an ESP32-S3 electronic chessboard with reed-switch matrix sensing, LED board feedback, WPA2 Enterprise STA networking, fixed SoftAP access, and a local web interface.

## Target

- MCU: ESP32-S3
- Framework: ESP-IDF v5.3.x
- Serial port: `/dev/ttyACM0`
- Required branch: `Breno`

## Network

The firmware must run in APSTA mode.

Fixed SoftAP:

- SSID: `XADREZ_ESP`
- Password: `xadrez12345`
- Web UI: `http://192.168.4.1/`

WPA2 Enterprise STA support must remain enabled. Do not commit real institutional credentials.

## Reed matrix

Rows 1 to 8:

`GPIO4, GPIO5, GPIO6, GPIO7, GPIO8, GPIO9, GPIO10, GPIO11`

Columns A to H:

`GPIO12, GPIO13, GPIO14, GPIO15, GPIO16, GPIO17, GPIO18, GPIO21`

Columns are outputs and are driven HIGH one at a time. Rows are inputs with pulldown. A HIGH row while scanning a column means a closed reed switch.

## LED strip

- Data GPIO: GPIO38
- Physical LED count: 104
- Used board LEDs: 64
- Skipped LEDs: 35

Physical order from the first LED:

`H1-H8, skip 5, G8-G1, skip 5, F1-F8, skip 5, E8-E1, skip 5, D1-D8, skip 5, C8-C1, skip 5, B1-B8, skip 5, A8-A1`

## Build

Build only:

`run chess build`

Build, flash, and monitor:

`run chess`

Monitor only:

`run chess monitor`

## Repository policy

Do not commit build output, managed components, logs, temporary files, or local credentials.

All code, comments, identifiers, logs, commit messages, file names, and technical documentation must be in English.


## WPA2 Enterprise credential provisioning

Institutional Wi-Fi credentials are not stored in source code.

Provision them locally with:

`run chess provision`

The command asks for SSID, EAP identity, username, and password in the terminal, creates an encrypted local archive under `secrets/`, generates a temporary NVS image, flashes it to the ESP32-S3 `nvs` partition, and deletes temporary plaintext files.

Do not commit `secrets/`, logs, generated NVS images, or real credentials.

## RTOS architecture

The firmware uses static FreeRTOS tasks, pinned to cores when dual-core FreeRTOS is enabled.

Runtime ownership model:

- `sensor_task`: scans the reed matrix and publishes `sensor_event_t` messages.
- `game_task`: owns chess state, board state, move validation, captures, infractions, check, checkmate, draw, and LED frame generation.
- `led_task`: consumes `led_command_t` frames and renders the LED strip.
- `network_task`: initializes APSTA networking and starts the local HTTP server.
- HTTP handlers must not directly mutate game state. They enqueue commands or read protected state snapshots.

Shared game state must be protected with `stateMutex`. New firmware behavior should preserve this ownership model.

## Source layout

- `main/include`: public component headers.
- `main/src/app`: application entry point and task creation.
- `main/src/chess`: chess rules and lightweight local engine.
- `main/src/drivers`: hardware drivers such as reed matrix and LED strip.
- `main/src/game`: game controller and state orchestration.
- `main/src/net`: networking, credentials, and provisioning support.
- `main/src/tools`: firmware-only utility modes such as LED mapping mode.
