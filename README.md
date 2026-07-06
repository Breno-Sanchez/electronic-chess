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
