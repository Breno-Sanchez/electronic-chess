# ESP32-S3 Electronic Chessboard

ESP32-S3 firmware for an electronic chessboard with reed-switch piece detection, addressable LED feedback, local chess-rule validation, APSTA networking, WPA2 Enterprise support, and a browser-based interface served from the board itself.

The board detects magnetic pieces through an 8x8 reed matrix, maintains the authoritative virtual chess position, validates moves, records FEN/PGN, renders legal moves and game status on LEDs, and exposes the current state at:

```text
http://192.168.4.1/
```

## Features

- ESP32-S3 firmware using ESP-IDF v5.3.x.
- 8x8 reed-switch matrix for physical piece presence.
- Local chess state with legal move validation, FEN, PGN, captures, promotion, check, checkmate, stalemate, draw flow, and chess clocks.
- Historical replay mode: select a stored historical game and replay only the expected PGN moves.
- Addressable LED feedback through the ESP32-S3 RMT peripheral.
- Fixed SoftAP interface for local access.
- WPA2 Enterprise STA support for institutional Wi-Fi.
- APSTA mode, allowing STA networking and SoftAP access at the same time.
- Runtime web configuration for LED colors, brightness, StockfishOnline enable/depth, clock settings, and invalid-position handling.
- Optional asynchronous StockfishOnline best-move hints.

## Hardware target

| Item | Value |
| --- | --- |
| MCU | ESP32-S3 |
| Framework | ESP-IDF v5.3.x |
| Serial port | `/dev/ttyACM0` |
| LED output | GPIO38 |
| Configured LED count | 150 |
| Used board LEDs | 64 |
| Skipped physical LEDs | 35 |

## Reed matrix pinout

The matrix scan model drives one column HIGH at a time. Rows are inputs with pulldown. A HIGH row reading during a column scan means that the corresponding reed switch is closed.

### Rows / ranks

| Board row | GPIO |
| --- | --- |
| Row 1 | GPIO4 |
| Row 2 | GPIO5 |
| Row 3 | GPIO6 |
| Row 4 | GPIO7 |
| Row 5 | GPIO8 |
| Row 6 | GPIO9 |
| Row 7 | GPIO10 |
| Row 8 | GPIO11 |

### Columns / files

| Board file | GPIO |
| --- | --- |
| Column A | GPIO12 |
| Column B | GPIO13 |
| Column C | GPIO14 |
| Column D | GPIO15 |
| Column E | GPIO16 |
| Column F | GPIO17 |
| Column G | GPIO18 |
| Column H | GPIO21 |

## LED strip mapping

The LED strip is configured for 150 physical LEDs. Only 64 LEDs represent chessboard squares; 35 intermediate LEDs are intentionally skipped because of the physical routing.

Physical order from the first LED:

```text
H1 to H8, skip 5 unused LEDs,
G8 to G1, skip 5 unused LEDs,
F1 to F8, skip 5 unused LEDs,
E8 to E1, skip 5 unused LEDs,
D1 to D8, skip 5 unused LEDs,
C8 to C1, skip 5 unused LEDs,
B1 to B8, skip 5 unused LEDs,
A8 to A1.
```

Default LED meanings:

| State | LED behavior |
| --- | --- |
| Empty square | Off by default |
| Piece present | Weak blue |
| Lifted origin | Blinking weak blue |
| Legal destination | Yellow |
| Best move / historical expected move | Green |
| Invalid move | Red |
| Check | Red board indication |
| Checkmate | Winner side green, loser side red |

## Network

The firmware runs in APSTA mode.

### Fixed SoftAP

| Setting | Value |
| --- | --- |
| SSID | `XADREZ_ESP` |
| Password | `xadrez12345` |
| Local web UI | `http://192.168.4.1/` |

### WPA2 Enterprise STA

WPA2 Enterprise support is kept in the firmware for institutional Wi-Fi. Real credentials must not be committed to the repository. Provision credentials locally through NVS.

## Configuration

Default runtime settings are embedded from:

```text
main/config.yaml
```

The firmware parses this file in C at runtime. Typical settings include SoftAP values, StockfishOnline defaults, LED GPIO/count/brightness, and default RGB values.

Runtime changes made through the web Configuration tab are stored in NVS.

## Build, flash, and monitor

The preferred project helper is:

```bash
run chess
```

It loads the ESP-IDF environment, enters the project directory, sets the target to `esp32s3`, builds, flashes to `/dev/ttyACM0`, and opens the serial monitor.

Build only:

```bash
run chess build
```

Flash and monitor:

```bash
run chess
```

Monitor only:

```bash
run chess monitor
```

Provision WPA2 Enterprise credentials:

```bash
run chess provision
```

This provisions SSID, EAP identity, username, and password locally into NVS. Do not commit generated credential files or real credentials.

## Source layout

```text
main/include       Public component headers
main/src/app       Application startup, runtime configuration, and task creation
main/src/chess     Chess rules, FEN/PGN, historical replay parsing, and board logic
main/src/drivers   Reed matrix scanner and LED strip renderer
main/src/game      Game controller, HTTP API, web UI, move orchestration, and LED command generation
main/src/net       Wi-Fi, credential provisioning, and StockfishOnline HTTP client
main/src/tools     Firmware utility modes
main/web/data      Embedded web data, including historical PGN replay data
```

## Main runtime tasks

| Task | Responsibility |
| --- | --- |
| `sensor_task` | Scans the reed matrix and publishes physical board events |
| `game_task` | Owns chess state, move validation, clocks, captures, draw/check/checkmate/stalemate logic, historical mode, and LED frame generation |
| `led_task` | Renders LED command frames on the physical strip |
| `network_task` | Initializes APSTA networking and starts the HTTP server |
| `stockfish_task` | Performs optional asynchronous StockfishOnline requests |

## Repository notes

- `main` is the stable public branch.
- Keep code, comments, identifiers, logs, commit messages, file names, and technical documentation in English.
- Do not commit `build/`, logs, generated NVS images, temporary files, local credential archives, or real institutional credentials.
- Keep WPA2 Enterprise support and the fixed SoftAP web interface.

## License

See `LICENSE`.
