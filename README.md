# simpl-slidrr-firmware

Firmware for **simpl-slirr** that emulates the **IO4 board + slider serial** on a single Raspberry Pi Pico.

> [!Warning]
> ### Disclosure of usage of Agentic AI Coding
> Much of this repo was vibe-coded with AI assistance. It builds and runs on real hardware, but treat it as a community fork — report issues if something breaks.

## About

Built on [whowechina/chu_pico](https://github.com/whowechina/chu_pico), with the native slider protocol from [skogaby/skogaslider](https://github.com/skogaby/skogaslider) and IO4 HID descriptors from [whowechina/chu_arcade](https://github.com/whowechina/chu_arcade).

### What changed from upstream chu_pico

- **Real slider and IO4 emulation only** — `joy` (RedBoard) and `nkro` HID modes removed; If you wanted keyboard output use slidershim
- **IR towers** for air keys — always on; no ToF support removed to simplicity
- **No RGB / WS2812** strips
- **Single-core** firmware (multicore removed because it was only running lights)
- **4th USB CDC** — binary **SMPL** config protocol for desktop tools ([`docs/protocol.md`](docs/protocol.md))

Pin wiring is unchanged from stock chu_pico — see [whowechina's docs](https://github.com/whowechina/chu_pico) for hardware.

## Quick start (games)

Assumes chu_pico-compatible wiring.

1. Flash `simpl-slidrr-firmware.uf2` onto your Pico.
2. **After a firmware update** (or first flash of this fork), open the CLI port and run **`factory`** to reset flash-backed config to defaults. Old flash may hold legacy values from upstream firmware.
3. In your game/tools `.ini`, disable software slider and IO4 emulation:

```
[slider]
enable=0

[io4]
enable=0
```

4. Launch the game.

> **Tip:** Windows Device Manager does not show CDC interface names. Use [Chrome Serial Terminal](https://googlechromelabs.github.io/serial-terminal) or the desktop GUI port dropdown to identify ports.

## USB interfaces

| Interface | String name | Purpose |
|-----------|-------------|---------|
| HID | *(IO4 board name goes here)* | Game input — air keys + aux buttons |
| CDC 0 | `simpl-slidrr CLI Port` | Text CLI (`stdio_usb`) |
| CDC 1 | `simpl-slidrr Slider Port` | Slider serial port |
| CDC 2 | `simpl-slidrr AIME Port` | NFC / AIME passthrough |
| CDC 3 | `simpl-slidrr config port` | SMPL binary config protocol |

## Building

Requires a [Pico SDK](https://www.raspberrypi.com/documentation/microcontrollers/c-sdk.html) dev environment.

```bash
git clone --recurse-submodules https://github.com/Infecta/simpl-slidrr-firmware
cd simpl-slidrr-firmware
mkdir build && cd build
cmake -G Ninja ..
ninja
```

Output: `build/src/simpl-slidrr-firmware.uf2`

## Configuration

### Text CLI

Connect to **`simpl-slidrr CLI Port`**. Type `?` for commands.

Common commands: `display`, `sense`, `filter`, `ir`, `delay`, `save`, `factory`, `recalculate`, `fps`.

Edits apply to RAM immediately; flash write is deferred ~5 s. Use **`save`** to persist now.

### Desktop GUI

PySide6 configurator in [`gui/`](gui/). Live telemetry, touch/IR tuning, device actions.

```bash
cd gui
pip install -r requirements.txt
python main.py
```

Auto-detects **`simpl-slidrr config port`**. On connect: loads config and streams telemetry at 60 Hz. **Apply** sends `SET_CONFIG`; **Save** persists to flash.

### Protocol / host tools

| Path | Role |
|------|------|
| [`docs/protocol.md`](docs/protocol.md) | SMPL wire format (`chu_cfg_t` = 64 bytes) |
| [`tools/smpl_protocol.py`](tools/smpl_protocol.py) | Shared Python library (GUI + tests) |
| [`tools/protocol_test.py`](tools/protocol_test.py) | Hardware smoke test |

```bash
pip install pyserial
python tools/protocol_test.py              # auto-detect config port
python tools/protocol_test.py COM10        # explicit port
```

## What works

- Native slider protocol (CDC 1)
- Native IO4 HID — air keys, Service, Test, Coin
- IR tower air sensing (6 sensors)
- MPR121 touch slider (32 keys)
- NFC / AIME passthrough (CDC 2)
- Flash-backed config via CLI, SMPL protocol, or GUI
- Text CLI (unchanged workflow from chu_pico)

## Credits

- [whowechina](https://github.com/whowechina) — [chu_pico](https://github.com/whowechina/chu_pico), [chu_arcade](https://github.com/whowechina/chu_arcade) IO4 descriptors
- [skogaby](https://github.com/skogaby) — native slider protocol from [skogaslider](https://github.com/skogaby/skogaslider)
