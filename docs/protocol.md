# simpl-slidrr Config Protocol (v1)

Binary request/response protocol on the **4th USB CDC** port (`simpl-slidrr config port`, TinyUSB CDC interface index **3**).

The text CLI remains on the **first** CDC port and is unchanged.

## USB layout

| CDC index | String descriptor | Purpose |
|-----------|-------------------|---------|
| 0 | `simpl-slidrr CLI Port` | Text CLI (`stdio_usb`) |
| 1 | `simpl-slidrr Slider Port` | SEGA IO4 slider serial |
| 2 | `simpl-slidrr AIME Port` | NFC / AIME passthrough |
| 3 | `simpl-slidrr config port` | This protocol |

Also present: **HID IO4 joystick** (64-byte report). VID/PID unchanged (`0x0ca3:0x0021`).

Firmware sources: `src/protocol.c`, `src/protocol.h`, `PROTO_CDC_ITF` in `protocol.h`.

## Frame format

All integers are **little-endian**.

| Offset | Size | Field |
|--------|------|-------|
| 0 | 4 | Magic: `'S' 'M' 'P' 'L'` (`0x4C504D53` as u32 LE) |
| 4 | 1 | Protocol version (`1`) |
| 5 | 1 | Opcode |
| 6 | 2 | Sequence (`seq`) — host picks; firmware echoes on response |
| 8 | 2 | Payload length |
| 10 | N | Payload |
| 10+N | 2 | CRC-16/CCITT |

CRC is computed over bytes `[0 .. 10+N-1]` with initial value `0xFFFF` and polynomial `0x1021`.

- Maximum payload: **256** bytes
- Maximum telemetry response frame: **231** bytes (219 payload + 10 header + 2 CRC)
- Malformed frames are discarded; the parser resyncs on the next `SMPL` magic
- Firmware chunks large TX writes to the CDC FIFO (`CFG_TUD_CDC_TX_BUFSIZE` = 256)

## Opcodes

| Opcode | Name | Request payload | Response payload |
|--------|------|-----------------|------------------|
| `0x01` | PING | empty | `proto_ver` u8, `fw_version` null-terminated string, `board_id` u64 |
| `0x10` | GET_CONFIG | empty | `cfg_version` u8, `chu_cfg_t` blob (64 bytes) |
| `0x11` | SET_CONFIG | `flags` u32, `chu_cfg_t` blob (64 bytes) | `status` u8; on success also `chu_cfg_t` blob |
| `0x20` | GET_TELEMETRY | empty | telemetry snapshot (219 bytes) |
| `0x21` | STREAM_ON | `rate_hz` u8 (1–60) | `status` u8 |
| `0x22` | STREAM_OFF | empty | `status` u8 |
| `0x30` | SAVE | empty | `status` u8 |
| `0x31` | FACTORY_RESET | empty | `status` u8 |
| `0x40` | RECALC_TOUCH | empty | `status` u8 |
| `0x41` | CAPTURE_IR_BASELINE | empty | `status` u8, `ir.base[6]` as six u16 LE |
| `0xA0` | TELEMETRY_PUSH | *(unsolicited, `seq` = 0)* | telemetry snapshot (219 bytes) |

### Status codes

| Value | Meaning |
|-------|---------|
| `0` | OK |
| `1` | Invalid payload |
| `2` | Validation failed |
| `3` | Busy |

On `SET_CONFIG` failure, response payload is **status only** (1 byte).

### SET_CONFIG section flags

Defined in `src/config.h`:

| Flag | Section |
|------|---------|
| `0x0001` | `sense` |
| `0x0002` | `hid` |
| `0x0004` | `aime` |
| `0x0008` | `ir` |
| `0x0010` | `tweak` (`delay_ms` only; `reserved` ignored) |

Unset sections in the patch are ignored. The full **64-byte** `chu_cfg_t` must still be sent; only flagged sections are applied.

Config mutations go through `config_apply_sections()` in `src/config.c` (same validation and side effects as CLI).

### `chu_cfg_t` wire layout (`cfg_version` = 1)

Packed struct, **64 bytes** on ARM GCC (`sizeof(chu_cfg_t)`). Enforced by `_Static_assert` in `src/config.c` — update host tools if this changes.

| Offset | Size | Field |
|--------|------|-------|
| 0 | 36 | `sense` — filter u8, global i8, debounce_touch u8, debounce_release u8, keys[32] i8 |
| 36 | 1 | `hid.io4` (bitfield, stored as u8) |
| 37 | 1 | `aime` — mode:4, virtual_aic:4 (stored as u8) |
| 38 | 12 | `ir.base[6]` u16 each |
| 50 | 6 | `ir.trigger[6]` u8 each (valid range 1–100) |
| 56 | 2 | `tweak.delay_ms` u16 (0–250) |
| 58 | 5 | `tweak.reserved` (padding; do not rely on values) |
| 63 | 1 | trailing padding to 64 bytes |

### Telemetry snapshot (219 bytes)

| Offset | Field |
|--------|-------|
| 0 | `frame_counter` u32 — increments each snapshot built by firmware (`static` RAM in `protocol.c`; not flash-backed; wraps at 2³²; resets on reboot) |
| 4 | `slider_raw[32]` u16 — keys 1A..16B order (same as CLI `raw`) |
| 68 | `touch_count[32]` u32 — cumulative touch counts (`stat` command) |
| 196 | `touch_bitmap` u32 — bit *i* set if key *i* currently touched |
| 200 | `ir_raw[6]` u16 — ADC values (`air_ir_raw`) |
| 212 | `ir_blocked[6]` u8 — 0/1 per sensor (`air_ir_blocked`) |
| 218 | `ir_bitmap` u8 — same as `air_bitmap()` |

`GET_TELEMETRY` and `STREAM_ON` both call `slider_raw()` (I2C read). Streaming above ~30 Hz may load the main loop. The reference GUI connects at **60 Hz**.

### Slider key index order (wire)

Firmware array order (index `i`):

| Index | Key |
|-------|-----|
| 0, 1 | 1A, 1B |
| 2, 3 | 2A, 2B |
| … | … |
| 30, 31 | 16A, 16B |

`touch_bitmap` bit *i* corresponds to the same index. Host UIs may display keys in physical panel layout (paired columns); that is a presentation concern only.

### AIR sensor index order (wire)

`ir_raw[i]` / `ir_blocked[i]` use firmware index `i` = 0..5 (CLI **IR1** .. **IR6**). Host UIs may reorder visually (e.g. label 6 at top); map indices accordingly.

## Flash persistence

- `SET_CONFIG` applies to RAM immediately and schedules a deferred flash write (~5 s), same as CLI edits.
- Send `SAVE` (`0x30`) to persist immediately.
- `FACTORY_RESET` (`0x31`) restores defaults and saves immediately.
- After firmware updates that change `chu_cfg_t`, run `FACTORY_RESET` or CLI `factory`.

## Host discovery

On Windows, open the COM port named **simpl-slidrr config port**. On Linux, match the USB interface with that string descriptor.

Baud rate is ignored (USB CDC); 115200 is fine for host tools.

## Host tools

### Shared library — `tools/smpl_protocol.py`

Python module used by the smoke test and desktop GUI:

- `ProtocolClient` — framing, CRC, transact, opcodes
- `ChuConfig` — pack/unpack 64-byte `chu_cfg_t`
- `Telemetry` — parse 219-byte snapshot
- `find_config_port()` — match **simpl-slidrr config port**

### Smoke test

```bash
pip install pyserial
python tools/protocol_test.py              # auto-detect config port
python tools/protocol_test.py COM10        # explicit port
python tools/protocol_test.py COM10 --skip-save
python tools/protocol_test.py COM10 --stream-hz 30 --stream-sec 1.0
```

Tests: PING, GET_CONFIG, GET_TELEMETRY, SET_CONFIG round-trip (`delay_ms`), CAPTURE_IR_BASELINE, STREAM_ON frame rate, SAVE.

**Verified:** all checks pass on hardware (2026-06-26).

### Desktop GUI — `gui/`

PySide6 (Qt) configurator. Imports `tools/smpl_protocol.py`.

```bash
cd gui
pip install -r requirements.txt
python main.py
```

On connect: PING, `GET_CONFIG`, `STREAM_ON` at 60 Hz. Config edits use `SET_CONFIG` section flags + toolbar **Apply**; **Save** sends `SAVE`. Device panel: `CAPTURE_IR_BASELINE`, `RECALC_TOUCH`, `FACTORY_RESET`.
