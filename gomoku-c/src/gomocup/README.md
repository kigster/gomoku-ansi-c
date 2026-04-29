# pbrain-kig-standard — Gomocup Brain

`pbrain-kig-standard` is a Gomoku AI brain that speaks the Gomocup AI Protocol v2 over stdin / stdout. It targets the **Standard category** of the Gomocup tournament, which fixes the board at 15x15 and requires exactly five in a row to win (overlines do not win).

- Brain name: `kig-standard`
- Version: `1.0.0`
- Author: Konstantin Gredeskoul (<kigster@gmail.com>) — <https://kig.re>
- Protocol: <https://plastovicka.github.io/protocl2en.htm>
- Tournament rules: <https://gomocup.org/detail-information/>

## What lives here

This directory (`gomoku-c/src/gomocup/`) is self-contained. Together with the shared engine in `gomoku-c/src/gomoku/` it builds a tournament-ready brain with no dependencies on json-c, the HTTP daemon, or any TUI / Web tooling.

```
gomoku-c/
|-- src/
|   |-- gomocup/
|   |   |-- main.c          stdin/stdout dispatch loop
|   |   |-- protocol.{c,h}  command parser (no engine state)
|   |   |-- coords.{c,h}    Gomocup [X],[Y] <-> engine (row, col)
|   |   |-- time_budget.{c,h} per-turn budget computation
|   |   |-- metadata.h      ABOUT line strings
|   |   `-- README.md       this file
|   `-- gomoku/             shared engine (linked with -DNO_JSON)
|-- tests/
|   |-- gomocup_test.cpp           parser + coord unit tests
|   `-- gomocup_protocol_e2e.sh    scripted-stdin integration test
`-- Makefile                       build & packaging recipes
```

## Build

### Native (macOS / Linux) — for development

```sh
cd gomoku-c
make pbrain-kig-standard
./bin/pbrain-kig-standard
```

The native binary is for development only. It speaks the same protocol as the Windows binaries, so you can drop into it interactively and exercise the parser:

```sh
cd gomoku-c
printf 'START 15\nABOUT\nBEGIN\nTURN 7,8\nEND\n' | ./bin/pbrain-kig-standard
```

### Windows (cross-compile from macOS / Linux) — for tournament submission

The two `.exe` files are the actual tournament artefacts. Both are statically linked so they run on a stock Windows install with no mingw runtime DLLs available.

```sh
brew install mingw-w64        # macOS; on Linux: apt install mingw-w64
cd gomoku-c
make gomocup-win
ls bin/pbrain-kig-standard64.exe bin/pbrain-kig-standard-x86.exe
```

Verify the .exe files have no third-party DLL dependencies:

```sh
x86_64-w64-mingw32-objdump -p bin/pbrain-kig-standard64.exe | grep "DLL Name"
# Should show only KERNEL32 + api-ms-win-crt-* (Windows 10/11 baseline UCRT).
```

## Run the tests

```sh
cd gomoku-c
make test                    # full suite: engine + daemon + gomocup parser + e2e
make test-gomocup-e2e        # just the scripted-stdin scenario
```

The `make test` run includes:

- 33 engine unit tests (`tests/gomoku_test.cpp`)
- 34 daemon unit tests (`tests/daemon_test.cpp`)
- 17 gomocup parser + coord unit tests (`tests/gomocup_test.cpp`)
- 4 scripted protocol scenarios (`tests/gomocup_protocol_e2e.sh`)

## Smoke-test the .exe files under Wine

If `wine` is available on the dev box, you can dry-run the cross-built binaries before packaging:

```sh
brew install --cask --no-quarantine wine-stable      # macOS only; install once
printf 'START 15\nBEGIN\nEND\n' | wine64 bin/pbrain-kig-standard64.exe
printf 'START 15\nBEGIN\nEND\n' | wine bin/pbrain-kig-standard-x86.exe
```

Wine is not strictly required — the Windows-VM step during tournament submission catches any wire-format issues. If `wine-stable` is too heavy on your machine, skip this step and rely on `make test-gomocup-e2e` against the native binary plus the official Piskvork manager on a Windows host.

## Package the submission .zip

```sh
cd gomoku-c
make gomocup-zip
ls bin/pbrain-kig-standard.zip
```

The zip contains:

- `pbrain-kig-standard64.exe` (Win64; the filename must contain the substring `64` per Gomocup rules)
- `pbrain-kig-standard-x86.exe` (Win32)
- `README.txt` stamped with the git SHA and build timestamp

Total size is a few hundred kilobytes — well under the 256 MB Gomocup ceiling.

## Submit to Gomocup

1. Sign in at <https://gomocup.org/>
2. Submit `bin/pbrain-kig-standard.zip` to the **Standard** category
3. Watch the result page for tournament games and crash logs

Before submitting, manually exercise the binaries against the Piskvork tournament manager on a Windows host to catch wire-format and behavioural regressions that the automated tests can miss. The Piskvork manager is downloadable from <https://gomocup.org/download-gomocup-manager/>.

## Protocol notes

- Board size is fixed at 15. The brain replies `ERROR unsupported board size` to `START n` for `n != 15`. To support 19x19 (Freestyle / Renju), the engine's compile-time `[19][19]` candidate buffers in `ai.c` would still cover it, but the protocol-level check here would need to widen.
- `RECTSTART`, `SWAP2BOARD`, and `PLAY` are recognised but declined with `ERROR <reason>`. They are not used in the Standard category.
- `INFO` keys honoured: `timeout_turn`, `timeout_match`, `time_left`. All other keys are silently consumed per spec.
- `time_left` from the manager is authoritative; the brain's internal estimate is overwritten on every `INFO time_left` it sees.
- The brain reserves a 200 ms safety margin against the manager's deadline so transmission delay or a context switch does not cause a forfeit.
- Default search depth is 7 with iterative deepening; default search radius is 3. The first move on `BEGIN` is the centre square (7, 7).

## Licence

Same licence as the parent `gomoku-c` project. See the repository root for details.
