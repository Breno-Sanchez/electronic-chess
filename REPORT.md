# ESP32-S3 Electronic Chessboard: Design and Implementation of a Networked Cyber-Physical Chess Interface

<p align="center">
  <a href="./README.md"><b>README</b></a> ·
  <a href="./REPORT.md"><b>REPORT</b></a> ·
  <a href="./LICENSE"><b>LICENSE</b></a>
</p>

**Project:** ESP32-S3 Electronic Chessboard
**Repository:** `Breno-Sanchez/electronic-chess`
**Target platform:** ESP32-S3
**Framework:** ESP-IDF v5.3.x
**Primary branch:** `main`
**Local web interface:** `http://192.168.4.1/`

---

## Abstract

This report presents the design, implementation, and evaluation of an ESP32-S3 electronic chessboard that integrates physical piece sensing, local chess-rule validation, addressable LED feedback, APSTA networking, a browser-based interface, and optional online move analysis. The system uses an 8x8 reed-switch matrix to detect magnetic pieces on a physical board and maps the resulting electrical states into algebraic chess coordinates. Firmware running on the ESP32-S3 maintains the authoritative virtual game state, validates moves locally, records FEN and PGN notation, controls game clocks, detects check, checkmate, stalemate, draw states, and renders visual feedback through an RMT-driven LED strip.

The project is structured as a cyber-physical embedded system: the physical board is the input surface, the firmware is the state authority, the LED strip is the immediate feedback channel, and the local web interface is the monitoring and configuration layer. The firmware also includes APSTA networking, keeping a fixed SoftAP at `192.168.4.1` while supporting WPA2 Enterprise as a station for institutional networks. A historical replay mode allows users to reproduce stored legendary games by following the exact PGN move sequence, with invalid deviations rejected by the rule layer. Results show that the board can perform complete interactive gameplay with local validation, visual guidance, state synchronization, runtime configuration, and educational replay functionality while preserving a compact firmware architecture based on explicit task ownership.

**Keywords:** ESP32-S3, electronic chessboard, reed switch matrix, embedded systems, APSTA, WPA2 Enterprise, RMT LED control, FEN, PGN, chess engine, historical replay.

---

## 1. Introduction

Electronic chessboards combine sensing, embedded control, and user feedback to bridge the gap between physical play and digital chess state management. Commercial electronic boards often rely on proprietary protocols, external software, or cloud services for move validation and visualization. This project explores a different approach: a self-contained ESP32-S3 firmware that can detect physical piece movement, validate chess rules locally, and serve a local web interface directly from the board.

The central design objective is to preserve the tactile experience of moving pieces on a real chessboard while adding digital assistance. The board detects which squares are occupied, maps those squares to algebraic notation, highlights legal destinations when a piece is lifted, rejects invalid moves, records FEN and PGN, and displays game state in a browser. The LED strip provides immediate board-level feedback, while the web UI gives a richer diagnostic and configuration interface.

The project also addresses practical embedded-system requirements. It uses a fixed SoftAP so the board is always reachable at `http://192.168.4.1/`, even when institutional Wi-Fi is unavailable. At the same time, it preserves WPA2 Enterprise station support for environments that require enterprise authentication. This dual-interface approach allows local interaction and optional internet access for StockfishOnline hints without making local gameplay dependent on cloud connectivity.

The current implementation targets the ESP32-S3 using ESP-IDF v5.3.x. The firmware is organized into separate modules for hardware drivers, chess logic, network support, game orchestration, runtime configuration, and web UI generation. The system uses FreeRTOS tasks with clear responsibility boundaries so that reed scanning, chess-state updates, LED rendering, HTTP handling, and optional StockfishOnline requests do not collapse into one monolithic control loop.

A major extension of the project is historical replay mode. In this mode, the firmware loads stored games from embedded JSON data, parses PGN moves, and guides the player through the expected sequence. The board behaves like a guided trainer: the next required move is shown, deviations are rejected, and the physical player must reproduce the game exactly. This mode transforms the board from a simple game validator into an educational replay platform.

---

## 2. System Overview

The electronic chessboard is composed of four primary layers:

1. **Physical sensing layer:** an 8x8 reed-switch matrix below the chessboard squares.
2. **Embedded control layer:** ESP32-S3 firmware that scans sensors, maintains chess state, validates moves, and manages runtime tasks.
3. **Visual feedback layer:** an addressable LED strip routed under or near the board squares.
4. **Network and UI layer:** APSTA networking with an HTTP server and browser-based interface.

The firmware treats the virtual chessboard as the authoritative source of truth once the game starts. The physical board is continuously scanned and interpreted as events: a square becomes occupied or empty. These events are processed by the game controller. When a valid piece is lifted, the firmware computes legal moves and renders them on both LEDs and the web UI. When a piece is placed, the firmware constructs a move candidate, validates it against chess rules, updates internal state if legal, or marks the involved squares invalid if the move is illegal.

The ESP32-S3 serves the web interface from the fixed SoftAP address. The browser UI mirrors the LED semantics so that the same concepts are visible both physically and digitally: piece presence, lifted origin, legal destinations, best move, invalid move, check, checkmate, draw, and historical expected moves.

---

## 3. Hardware Architecture

### 3.1 Microcontroller

The target controller is the ESP32-S3. It provides sufficient GPIO availability for the reed matrix, RMT support for LED output, Wi-Fi support for APSTA networking, and FreeRTOS integration through ESP-IDF.

| Parameter | Value |
| --- | --- |
| MCU | ESP32-S3 |
| Framework | ESP-IDF v5.3.x |
| Serial port | `/dev/ttyACM0` |
| LED output | GPIO38 |
| SoftAP address | `192.168.4.1` |

### 3.2 Reed-Switch Matrix

The board uses 64 reed switches, one per chess square. Magnetic pieces close the switches when present. The matrix scan model is intentionally simple:

- Columns are outputs.
- Rows are inputs with pulldown.
- One column is driven HIGH at a time.
- A HIGH row value during a column scan indicates a closed switch at the active column and row.

Rows / ranks 1 to 8:

| Rank line | GPIO |
| --- | --- |
| Row 1 | GPIO4 |
| Row 2 | GPIO5 |
| Row 3 | GPIO6 |
| Row 4 | GPIO7 |
| Row 5 | GPIO8 |
| Row 6 | GPIO9 |
| Row 7 | GPIO10 |
| Row 8 | GPIO11 |

Columns / files A to H:

| File line | GPIO |
| --- | --- |
| Column A | GPIO12 |
| Column B | GPIO13 |
| Column C | GPIO14 |
| Column D | GPIO15 |
| Column E | GPIO16 |
| Column F | GPIO17 |
| Column G | GPIO18 |
| Column H | GPIO21 |

The mapping is explicit in firmware so that physical wiring changes can be corrected without rewriting chess logic. The chess logic uses algebraic square names such as `a1`, `e4`, and `h8`, while the driver layer deals with row and column GPIOs.

### 3.3 LED Strip

The LED strip is driven through the ESP32-S3 RMT peripheral. The current firmware uses GPIO38 and configures 150 physical LEDs. Of these, 64 represent chessboard squares and 35 are intentionally skipped between files due to the physical routing.

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

The LED driver translates algebraic board-square commands into this physical LED order. This separation allows chess logic to remain independent from strip routing.

---

## 4. Firmware Architecture

The firmware is divided into modules according to responsibility. This structure keeps the system maintainable and reduces cross-layer dependencies.

```text
main/include       Public component headers
main/src/app       Application startup, runtime configuration, task creation
main/src/chess     Chess rules, FEN/PGN, historical replay parsing
main/src/drivers   Reed matrix scanner and LED strip renderer
main/src/game      Game controller, HTTP API, UI, move orchestration
main/src/net       Wi-Fi, credential provisioning, StockfishOnline client
main/src/tools     Firmware utility modes
main/web/data      Embedded data such as historical replay JSON
```

### 4.1 FreeRTOS Task Model

The firmware uses static task responsibilities rather than a single all-purpose loop.

| Task | Responsibility |
| --- | --- |
| `sensor_task` | Scans the reed matrix and publishes sensor events. |
| `game_task` | Owns chess state, move validation, captures, clocks, historical mode, and LED frame generation. |
| `led_task` | Renders LED command frames on the physical strip. |
| `network_task` | Initializes APSTA networking and starts the HTTP server. |
| `stockfish_task` | Performs optional asynchronous StockfishOnline requests. |

The `game_task` owns authoritative game state. HTTP handlers expose controlled access through state snapshots and protected actions. Hardware drivers do not directly mutate chess rules or game state.

### 4.2 State Protection

Shared game data is protected through `stateMutex`. This is important because HTTP handlers, Stockfish responses, game events, and UI polling can happen concurrently. The design principle is that the game controller owns mutation, while other tasks either publish events, consume LED frames, or request snapshots.

### 4.3 Runtime Configuration

Default configuration is embedded from `main/config.yaml` and parsed in C at runtime. The configuration includes SoftAP data, Stockfish defaults, LED GPIO/count/brightness, and LED colors. Runtime changes made from the web UI are stored in NVS, allowing user preferences to survive reboot without recompiling firmware.

Current default examples include:

```text
target: esp32s3
serial_port: /dev/ttyACM0
softap_ssid: XADREZ_ESP
softap_password: xadrez12345
led_gpio: 38
led_physical_count: 150
stockfish_depth: 10
```

Real institutional WPA2 Enterprise credentials are not stored in the repository. They are provisioned locally into NVS.

---

## 5. Methodology

### 5.1 Sensor Scanning Method

The reed matrix is scanned by selecting one column at a time and reading the row inputs. Since only one column is active during each scan interval, a row HIGH value can be mapped to a single square. The resulting row/column state is converted into a board-square occupancy map.

The scanner reports changes rather than continuously pushing full board states. This event-driven model reduces unnecessary processing and allows the game layer to reason about meaningful transitions:

- `PRESENT`: a piece or magnet is detected on a square.
- `REMOVED`: a previously present piece is removed from a square.

The game controller consumes these transitions to detect lift and placement operations.

### 5.2 Game Start and Setup Verification

Before normal gameplay starts, the firmware checks whether the physical occupancy matches the expected initial chess position. This prevents the virtual board from starting in a standard FEN while the physical pieces are misplaced. If the board is not ready, the firmware remains in setup mode and provides visual feedback.

When the initial setup is valid, the firmware resets the internal chess board, initializes clocks, clears stale hints, resets move state, and enters a running game mode.

### 5.3 Move Interpretation

A physical move is interpreted in two phases:

1. **Lift phase:** a piece is removed from a square. If the piece belongs to the side to move, the square becomes the selected origin.
2. **Placement phase:** a piece appears on a destination square. The firmware constructs a move candidate and validates it.

If the move is legal, the internal board is updated, PGN is extended, FEN is regenerated, clocks are updated, and LED/UI state is refreshed. If the move is illegal, the involved squares are marked invalid.

This physical two-phase method maps naturally to real chess behavior while still allowing the firmware to reject impossible moves.

### 5.4 Local Chess Rules

Move legality is determined locally. This is a key design decision: network access and StockfishOnline availability are not required for the board to enforce chess rules. The local rules layer tracks:

- Piece placement.
- Side to move.
- Legal piece movement.
- Captures.
- Castling rights.
- En passant target.
- Promotion.
- Halfmove and fullmove counters.
- Check and checkmate.
- Stalemate.
- Draw by agreement.

FEN and PGN are updated as game-state outputs. FEN provides a compact authoritative snapshot, while PGN records the played sequence.

### 5.5 Historical Replay Method

Historical replay mode embeds a JSON file containing selected games. Each game includes metadata such as event, site, date, player names, result, and a PGN-like move string. At startup, the firmware loads and parses this data into a compact internal representation.

During replay, the firmware expects exactly one move at a time. The expected move is rendered as a green hint and exposed in the UI. If the player moves a different piece or selects a different destination, the move is rejected. After the correct move is completed, the replay advances to the next PGN ply.

This mode required careful integration with normal gameplay because stale best-move and historical hint states must be cleared when switching between normal and historical modes. The final implementation explicitly clears historical state when normal Start is pressed and clears stale advisor responses when entering or leaving replay mode.

### 5.6 LED Rendering Method

The LED renderer receives compact board-state maps from the game controller. Each LED frame can represent multiple semantic overlays:

- Physical piece presence.
- Lifted origin.
- Legal destination.
- Best move or historical expected move.
- Invalid move.
- Check/checkmate/draw terminal states.

The renderer applies priority rules so that critical states such as invalid moves and check are not hidden by lower-priority piece presence.

### 5.7 Web Interface Method

The web interface is served by the ESP32 HTTP server. It is designed as a local diagnostic and interaction layer rather than a cloud application. The UI polls firmware APIs, renders the board, shows FEN/PGN/state data, exposes configuration controls, and provides special flows such as draw proposals, promotion selection, clock configuration, Stockfish toggles, and historical game selection.

The UI mirrors LED semantics. This makes debugging easier because the same logical board overlays are visible in the browser and on the physical board.

### 5.8 Network Method

The firmware uses APSTA mode:

- SoftAP provides deterministic local access.
- STA provides institutional Wi-Fi access when credentials are provisioned.
- WPA2 Enterprise support is preserved.
- Real credentials are stored only through local NVS provisioning.

The fixed SoftAP is essential because it provides a fallback control path even if the STA interface fails or cannot authenticate.

### 5.9 StockfishOnline Method

StockfishOnline integration is optional. The firmware can asynchronously request a best move for the current FEN at a configurable depth. Responses are treated as advisory only. They do not affect legality. When disabled, pending requests and green best-move hints are cleared.

This separation avoids making the board dependent on internet access and keeps local game correctness under firmware control.

---

## 6. Demonstration Video

The following video demonstrates physical board interaction, LED feedback, and the web interface during gameplay.

https://github.com/user-attachments/assets/bd02f91b-ea6f-4f28-a71c-ffd5dfbfd848

Local repository fallback, when the MP4 is committed under `docs/media/xadrez.mp4`:

<video src="docs/media/xadrez.mp4" width="720" controls></video>

---

## 7. Results and Discussion

### 7.1 Physical Board Detection

The reed-switch matrix successfully detects square presence and removal events. The event logs show algebraic square names, which confirms that low-level GPIO readings are translated into chess coordinates before reaching the game controller. This makes runtime diagnostics readable and simplifies debugging.

The physical detection model works best when magnetic pieces are aligned with their square sensors. Mechanical alignment remains important because reed switches are sensitive to magnet position and field strength. The firmware can debounce and validate transitions, but stable hardware placement is still necessary for a reliable user experience.

### 7.2 Gameplay Validation

The firmware performs complete local validation for ordinary moves, captures, promotion, check, checkmate, draw flow, and stalemate. Because move legality is local, the board remains usable even when StockfishOnline is disabled or unavailable.

The two-phase lift/place model is appropriate for a physical board. It allows the system to highlight legal destinations immediately after a valid piece is lifted. If the player returns the piece to the origin, the board returns to running mode without changing FEN or PGN. If the player places the piece on an illegal square, the firmware rejects the move and marks the involved squares invalid.

### 7.3 LED Feedback

LED feedback provides immediate interaction support. Weak blue piece presence helps visualize detected occupancy. Yellow legal destinations guide the current move. Green best-move or historical-expected indicators support analysis and replay. Red invalid indicators make rejected moves visible without requiring the user to inspect serial logs.

The custom physical LED order was successfully abstracted away from game logic. This is important because the physical strip routing is not a simple row-major or file-major mapping. The firmware maintains algebraic square semantics internally and only converts to strip indices inside the LED rendering layer.

### 7.4 Web Interface

The web interface improves usability and diagnostics. It shows the board, current side to move, FEN, PGN, legal moves, captured material, clocks, configuration, draw state, Stockfish JSON, and historical replay controls. This makes the board usable without external desktop software.

The interface also helped reveal integration problems during development. For example, frequent HTTP requests exposed stack limitations in the HTTP server task after historical mode was added. The solution was to avoid heavy historical parsing inside HTTP handlers and preload historical data in the game task, which has a larger stack and clearer ownership.

### 7.5 Historical Replay Mode

Historical replay mode extends the project from a gameplay validator into a training and demonstration tool. The user can select a stored game and reproduce it physically. The firmware rejects any move that does not match the expected PGN sequence.

This mode produced several important engineering lessons:

- Replay state must be isolated from normal game state.
- Stale historical hints must be cleared when normal Start is pressed.
- HTTP handlers should not perform heavy parsing on embedded systems with small stacks.
- Historical data should be preloaded and cached.
- Expected moves should use the same LED and UI rendering pipeline as best-move hints, but must be cleared when leaving replay mode.

After these corrections, historical mode operates consistently and no longer leaves stale green hints after returning to normal gameplay.

### 7.6 Network Behavior

The APSTA architecture satisfies both local usability and institutional network access. The fixed SoftAP gives a predictable control address. STA mode with WPA2 Enterprise allows internet access for optional StockfishOnline analysis. The board does not require internet for core chess operation.

The project treats network services as optional from the perspective of chess legality. This is a robust embedded-systems choice because local physical operation should not depend on a remote service.

### 7.7 Runtime Configuration

Runtime configuration reduces compile/flash cycles. LED colors, brightness, empty-square LEDs, invalid-position handling, Stockfish settings, and clocks can be adjusted from the browser. Values persist through NVS storage.

The configuration system also allows the firmware to keep defaults in `main/config.yaml`, making default behavior visible in source control while still permitting local runtime changes.

### 7.8 Timing and Clocks

Chess clocks are integrated into game state. The side to move counts down while the game is active. Time continues during lifted and promotion-pending states, and the configured increment is added after legal moves. Terminal states distinguish white and black time losses.

This feature makes the board suitable for practical timed play and also verifies that the state machine can handle non-move terminal transitions.

### 7.9 Maintainability

The project structure supports incremental development. Drivers, chess logic, network code, game orchestration, and UI generation are separated. This is especially important because embedded chessboard behavior crosses multiple domains: GPIO scanning, time-sensitive LED rendering, HTTP serving, NVS storage, Wi-Fi, and chess rules.

The current architecture is compact but still expressive enough to support new features such as historical replay without broad rewrites.

---

## 8. Build and Execution

The preferred helper command is:

```bash
run chess
```

This command loads the ESP-IDF environment, enters the project directory, sets the target to `esp32s3`, builds the firmware, flashes `/dev/ttyACM0`, and opens the serial monitor.

Build only:

```bash
run chess build
```

Monitor only:

```bash
run chess monitor
```

Provision WPA2 Enterprise credentials:

```bash
run chess provision
```

Real institutional credentials must not be committed. Provisioning should remain local.

---

## 9. Limitations

The current implementation is functional, but several limitations remain:

1. **Mechanical dependency on magnet alignment:** reed switches require consistent piece magnet placement.
2. **HTTP server stack sensitivity:** heavy parsing or large local buffers must be avoided inside HTTP handlers.
3. **StockfishOnline dependency for best-move hints:** best-move analysis is optional and depends on internet access.
4. **No persistent PGN export file yet:** PGN is available in the UI but not yet persisted as downloadable match history.
5. **Historical replay dataset is embedded:** adding games currently requires modifying embedded JSON and rebuilding firmware.
6. **Limited browser-side asset budget:** the web UI must stay compact to fit embedded serving constraints.
7. **No per-square calibration screen yet:** sensor diagnostics exist through logs and UI state but can be improved.

These limitations are acceptable for the current phase because the priority is correct hardware behavior, stable APSTA operation, compact firmware size, and maintainable embedded code.

---

## 10. Future Work

Future improvements should preserve the current modular architecture and avoid unnecessary dependencies.

Recommended next steps:

- Add a historical game management format with validated PGN import tooling.
- Add persistent PGN export through the web UI.
- Add per-square reed sensor calibration and diagnostics.
- Add an LED mapping diagnostic page.
- Add a compact mobile-first UI mode.
- Add accessibility color presets for different visual conditions.
- Add optional local sound feedback for invalid move, check, and game end.
- Add firmware self-test routines for matrix rows, columns, and LED strip continuity.
- Add replay speed or guided-study annotations for historical games.
- Add a small offline fallback evaluator only if firmware size remains acceptable.

---

## 11. Conclusion

The ESP32-S3 electronic chessboard demonstrates a complete embedded cyber-physical chess system. It detects physical piece movement with a reed-switch matrix, validates chess moves locally, records FEN and PGN, controls LED feedback, serves a browser UI, supports APSTA networking, preserves WPA2 Enterprise compatibility, and optionally requests StockfishOnline hints. The historical replay mode adds an educational dimension by guiding users through stored PGN games and rejecting deviations.

The final architecture emphasizes local correctness, clear task ownership, stable hardware behavior, and minimal dependencies. The system remains usable without internet access, while still supporting optional online analysis when STA networking is available. The result is a practical embedded chess platform suitable for demonstrations, learning, gameplay assistance, and further firmware experimentation.

---

## References

1. Espressif Systems. *ESP-IDF Programming Guide*. ESP32-S3 documentation for build system, FreeRTOS integration, Wi-Fi, HTTP server, NVS, and RMT peripherals.
2. Espressif Systems. *ESP32-S3 Technical Reference Manual*. Hardware reference for GPIO, Wi-Fi, RMT, and SoC peripherals.
3. FreeRTOS. *FreeRTOS Kernel Documentation*. Task scheduling, queues, synchronization primitives, and stack management concepts.
4. FIDE. *Laws of Chess*. Rule reference for move legality, check, checkmate, stalemate, castling, promotion, and game termination.
5. Steven J. Edwards. *Forsyth-Edwards Notation Specification*. Standard representation for chess positions.
6. Portable Game Notation Specification and Implementation Guide. Standard notation for recording chess games.
7. Stockfish project. *Stockfish chess engine documentation and UCI concepts*. Reference for chess-engine move analysis concepts.
8. StockfishOnline API. Online move-analysis service used as an optional advisor by the firmware.
9. Lichess. *cburnett chess piece set*. SVG chess piece assets used as a visual reference for browser board rendering.
10. Espressif Systems. *WPA2 Enterprise examples and Wi-Fi station documentation*. Reference for enterprise authentication support on ESP-IDF.
