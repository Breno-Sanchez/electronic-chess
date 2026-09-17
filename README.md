# ESP32-S3 Electronic Chessboard

<p align="center">
  <a href="./REPORT.md"><b>Technical Report</b></a> ·
  <a href="./LICENSE"><b>License</b></a>
</p>

Firmware for an ESP32-S3 electronic chessboard with reed-switch piece detection, addressable LED feedback, local chess move validation, APSTA networking, WPA2 Enterprise support, a fixed SoftAP web interface, runtime configuration, historical game replay, chess clocks, and optional StockfishOnline move hints.

The board detects magnetic chess pieces through an 8x8 reed-switch matrix. The ESP32-S3 keeps the authoritative virtual chess state, validates moves locally, updates FEN/PGN, drives the LEDs, and serves a browser interface at:

```text
http://192.168.4.1/
```

## Main features

- ESP32-S3 firmware using ESP-IDF v5.3.x.
- 8x8 reed-switch matrix for physical square occupancy.
- Local chess rules, move validation, captures, promotion, castling, en passant, check, checkmate, stalemate, draw flow, and FEN/PGN tracking.
- Historical game replay mode using embedded PGN data.
- Chess clocks with configurable base time and increment.
- WS2812-style LED feedback through the ESP32-S3 RMT peripheral.
- Fixed SoftAP interface at `http://192.168.4.1/`.
- WPA2 Enterprise STA support for institutional Wi-Fi.
- APSTA mode: SoftAP and STA run at the same time.
- Runtime configuration for LED brightness, colors, empty-square LEDs, Stockfish enable/disable, Stockfish depth, clock time, and clock increment.
- Optional asynchronous StockfishOnline advisor.
- Browser UI with board view, physical presence map, FEN, PGN, legal moves, best move, captured material, clocks, historical replay controls, and configuration controls.

## Hardware target

| Item | Value |
| --- | --- |
| MCU | ESP32-S3 |
| Framework | ESP-IDF v5.3.x |
| Default serial port | `/dev/ttyACM0` |
| LED output | GPIO38 |
| Local web UI | `http://192.168.4.1/` |

## Reed-switch matrix pinout

The matrix scan drives one column HIGH at a time and reads the rows as pulldown inputs. A HIGH row reading means the reed switch at that square is closed.

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

| Board column | GPIO |
| --- | --- |
| Column A | GPIO12 |
| Column B | GPIO13 |
| Column C | GPIO14 |
| Column D | GPIO15 |
| Column E | GPIO16 |
| Column F | GPIO17 |
| Column G | GPIO18 |
| Column H | GPIO21 |

## LED strip

The LED strip is driven by the ESP32-S3 RMT peripheral.

| Item | Value |
| --- | --- |
| Data GPIO | GPIO38 |
| Used board LEDs | 64 |
| Skipped LEDs | 35 |
| Minimum physical LEDs | 99 |
| Configured physical LEDs | 150 |

Physical LED order from the first LED:

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

Default LED behavior:

| Board state | LED behavior |
| --- | --- |
| Empty square | Off by default, configurable color when enabled |
| Piece present | Weak blue |
| Lifted origin square | Blinking weak blue |
| Legal moves | Yellow |
| Stockfish or historical expected move | Green |
| Invalid move | Red |
| Check | Red check indication |
| Draw | Configurable draw color |
| Checkmate | Winner side blinks green, loser side blinks red |

## Network behavior

The firmware runs in APSTA mode.

### Fixed SoftAP

| Setting | Value |
| --- | --- |
| SSID | `XADREZ_ESP` |
| Password | `xadrez12345` |
| Local UI | `http://192.168.4.1/` |

The SoftAP remains available even when the STA interface is not connected.

### WPA2 Enterprise STA

WPA2 Enterprise support is enabled for institutional Wi-Fi. Real credentials must not be stored in source code or committed to the repository. Credentials are provisioned locally into NVS.

## How to run the project

### 1. Install ESP-IDF

Install ESP-IDF v5.3.x and load the ESP-IDF environment in the current terminal before building.

Example for a standard ESP-IDF installation on Linux:

```bash
source "$HOME/esp/esp-idf/export.sh"
```

Adjust the path if ESP-IDF is installed somewhere else.

### 2. Clone the repository

```bash
git clone https://github.com/Breno-Sanchez/electronic-chess.git
cd electronic-chess
```

### 3. Select the ESP32-S3 target

```bash
idf.py set-target esp32s3
```

### 4. Build the firmware

```bash
idf.py build
```

### 5. Flash and monitor

Connect the ESP32-S3 board over USB and select the correct serial port.

Default Linux port used by this project:

```bash
idf.py -p /dev/ttyACM0 flash monitor
```

If the board appears on another port, replace `/dev/ttyACM0` with the detected port.

Common examples:

```bash
idf.py -p /dev/ttyUSB0 flash monitor
idf.py -p COM3 flash monitor
```

Press `Ctrl+]` to exit the ESP-IDF serial monitor.

### 6. Open the web interface

After flashing, connect a phone or computer to the board SoftAP:

```text
SSID: XADREZ_ESP
Password: xadrez12345
```

Then open:

```text
http://192.168.4.1/
```

### Optional helper scripts

The repository includes helper scripts under `scripts/`. They are optional and are mainly intended for local development convenience.

Build using the project helper:

```bash
./scripts/run-chess.sh build
```

Build, flash, and monitor using the project helper:

```bash
./scripts/run-chess.sh
```

Provision WPA2 Enterprise credentials into NVS:

```bash
./scripts/run-chess.sh provision
```

The helper assumes the project's default serial port unless configured otherwise. The standard `idf.py` commands above are the generic way to build and run the project on any machine.

## Source layout

```text
main/include       Public component headers
main/src/app       Application startup, runtime configuration, and task creation
main/src/chess     Chess rules, board state, FEN/PGN, historical games, and move validation
main/src/drivers   Reed matrix scanner and LED strip renderer
main/src/game      Game controller, HTTP API, web UI, state orchestration, and LED command generation
main/src/net       Wi-Fi, credential provisioning, and StockfishOnline HTTP client
main/src/tools     Firmware utility modes
main/web/data      Embedded historical game data
scripts            Optional local build, flash, monitor, and provisioning helpers
```

## Runtime configuration

Default configuration is stored in:

```text
main/config.yaml
```

Runtime settings are stored in NVS and can be changed from the web Configuration tab. Typical runtime settings include LED brightness, LED colors, empty-square LEDs, StockfishOnline enable/disable, Stockfish depth, chess clock time, and chess clock increment.

## Security notes

- Do not commit real institutional Wi-Fi credentials.
- Keep local credential artifacts out of Git.
- The fixed SoftAP password is part of the project configuration, but WPA2 Enterprise STA credentials must be provisioned locally.
- StockfishOnline is optional and can be disabled at runtime.

## License

See [LICENSE](./LICENSE).
