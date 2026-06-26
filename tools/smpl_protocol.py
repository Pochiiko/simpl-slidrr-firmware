"""simpl-slidrr SMPL config protocol — shared host library."""

from __future__ import annotations

import re
import struct
import time
from dataclasses import dataclass, field
from typing import Iterator

try:
    import serial
    import serial.tools.list_ports
except ImportError:  # pragma: no cover
    serial = None  # type: ignore[assignment]

MAGIC = b"SMPL"
PROTO_VERSION = 1
CFG_BLOB_SIZE = 64
TELEMETRY_SIZE = 219

OP_PING = 0x01
OP_GET_CONFIG = 0x10
OP_SET_CONFIG = 0x11
OP_GET_TELEMETRY = 0x20
OP_STREAM_ON = 0x21
OP_STREAM_OFF = 0x22
OP_SAVE = 0x30
OP_FACTORY_RESET = 0x31
OP_RECALC_TOUCH = 0x40
OP_CAPTURE_IR_BASE = 0x41
OP_TELEMETRY_PUSH = 0xA0

CFG_SECTION_SENSE = 0x0001
CFG_SECTION_HID = 0x0002
CFG_SECTION_AIME = 0x0004
CFG_SECTION_IR = 0x0008
CFG_SECTION_TWEAK = 0x0010

STATUS_OK = 0
STATUS_INVALID = 1
STATUS_VALIDATE = 2
STATUS_BUSY = 3

KEY_LABELS = [f"{i}{side}" for i in range(1, 17) for side in ("A", "B")]

USB_VID = 0x0CA3
USB_PID = 0x0021

# Windows exposes generic "USB Serial Device" for usbser.sys; pyserial.interface is
# unset on Windows. COM ports map to a USB interface number (MI_xx) encoded in
# ListPortInfo.location as ":x.N" — see pyserial list_ports_windows.py.
# Names match string_desc_arr[4..7] in src/usb_descriptors.c.
CDC_INTERFACE_NAMES: dict[int, str] = {
    1: "simpl-slidrr CLI Port",
    3: "simpl-slidrr Slider Port",
    5: "simpl-slidrr AIME Port",
    7: "simpl-slidrr config port",
}
CONFIG_CDC_INTERFACE = 7


def _usb_interface_number(port: serial.tools.list_ports.ListPortInfo) -> int | None:
    if port.location:
        m = re.search(r":x\.(\d+)$", port.location)
        if m:
            return int(m.group(1))
    if port.hwid:
        m = re.search(r"MI_(\d+)", port.hwid, re.I)
        if m:
            return int(m.group(1))
    return None


def _port_label(port: serial.tools.list_ports.ListPortInfo) -> str:
    if port.interface:
        return port.interface
    if port.vid == USB_VID and port.pid == USB_PID:
        mi = _usb_interface_number(port)
        if mi is not None:
            name = CDC_INTERFACE_NAMES.get(mi)
            if name:
                return name
    return port.description or port.device


def _port_search_text(port: serial.tools.list_ports.ListPortInfo) -> str:
    parts = (_port_label(port), port.interface, port.description, port.product, port.manufacturer)
    return " ".join(p for p in parts if p).lower()


def crc16_ccitt(data: bytes) -> int:
    crc = 0xFFFF
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


def build_frame(opcode: int, seq: int, payload: bytes = b"") -> bytes:
    hdr = MAGIC + bytes([PROTO_VERSION, opcode]) + struct.pack("<HH", seq, len(payload))
    body = hdr + payload
    return body + struct.pack("<H", crc16_ccitt(body))


def parse_frames(buf: bytearray) -> Iterator[tuple[int, int, bytes]]:
    while True:
        idx = buf.find(MAGIC)
        if idx < 0:
            buf.clear()
            return
        if idx > 0:
            del buf[:idx]
        if len(buf) < 12:
            return
        plen = struct.unpack_from("<H", buf, 8)[0]
        total = 10 + plen + 2
        if len(buf) < total:
            return
        frame = bytes(buf[:total])
        del buf[:total]
        expect = crc16_ccitt(frame[:-2])
        got = struct.unpack_from("<H", frame, 10 + plen)[0]
        if expect != got:
            continue
        opcode = frame[5]
        seq = struct.unpack_from("<H", frame, 6)[0]
        payload = frame[10 : 10 + plen]
        yield opcode, seq, payload


def find_config_port() -> str | None:
    if serial is None:
        return None
    for p in serial.tools.list_ports.comports():
        if p.vid == USB_VID and p.pid == USB_PID:
            mi = _usb_interface_number(p)
            if mi == CONFIG_CDC_INTERFACE:
                return p.device
        if "config port" in _port_search_text(p):
            return p.device
    return None


def list_serial_ports() -> list[tuple[str, str]]:
    if serial is None:
        return []
    all_ports = list(serial.tools.list_ports.comports())
    ours = [p for p in all_ports if p.vid == USB_VID and p.pid == USB_PID]
    candidates = ours if ours else all_ports
    ports = [(p.device, _port_label(p)) for p in candidates]
    ports.sort(key=lambda item: item[1].lower())
    return ports


@dataclass
class ChuConfig:
    filter_byte: int = 0x10
    global_sense: int = 0
    debounce_touch: int = 1
    debounce_release: int = 0
    keys: list[int] = field(default_factory=lambda: [0] * 32)
    hid_io4: bool = True
    aime_mode: int = 0
    aime_virtual: bool = False
    ir_base: list[int] = field(default_factory=lambda: [3800] * 6)
    ir_trigger: list[int] = field(default_factory=lambda: [20] * 6)
    delay_ms: int = 0

    @property
    def filter_ffi(self) -> int:
        return self.filter_byte >> 6

    @property
    def filter_sfi(self) -> int:
        return (self.filter_byte >> 4) & 0x03

    @property
    def filter_esi(self) -> int:
        return self.filter_byte & 0x07

    def set_filter(self, ffi: int, sfi: int, esi: int) -> None:
        self.filter_byte = (ffi << 6) | (sfi << 4) | esi

    @classmethod
    def from_bytes(cls, blob: bytes) -> ChuConfig:
        if len(blob) != CFG_BLOB_SIZE:
            raise ValueError(f"cfg blob must be {CFG_BLOB_SIZE} bytes, got {len(blob)}")
        keys = list(struct.unpack_from("<32b", blob, 4))
        aime_byte = blob[37]
        return cls(
            filter_byte=blob[0],
            global_sense=struct.unpack_from("<b", blob, 1)[0],
            debounce_touch=blob[2],
            debounce_release=blob[3],
            keys=keys,
            hid_io4=bool(blob[36] & 1),
            aime_mode=aime_byte & 0x0F,
            aime_virtual=bool((aime_byte >> 4) & 1),
            ir_base=[struct.unpack_from("<H", blob, 38 + i * 2)[0] for i in range(6)],
            ir_trigger=[blob[50 + i] for i in range(6)],
            delay_ms=struct.unpack_from("<H", blob, 56)[0],
        )

    def sense_fingerprint(self) -> tuple:
        return (
            self.filter_byte,
            self.global_sense,
            self.debounce_touch,
            self.debounce_release,
            tuple(self.keys),
        )

    def to_bytes(self) -> bytes:
        blob = bytearray(CFG_BLOB_SIZE)
        blob[0] = self.filter_byte & 0xFF
        struct.pack_into("<b", blob, 1, self.global_sense)
        blob[2] = self.debounce_touch & 0xFF
        blob[3] = self.debounce_release & 0xFF
        for i, k in enumerate(self.keys):
            struct.pack_into("<b", blob, 4 + i, k)
        blob[36] = 1 if self.hid_io4 else 0
        blob[37] = (self.aime_mode & 0x0F) | ((1 if self.aime_virtual else 0) << 4)
        for i, v in enumerate(self.ir_base):
            struct.pack_into("<H", blob, 38 + i * 2, v)
        for i, v in enumerate(self.ir_trigger):
            blob[50 + i] = v & 0xFF
        struct.pack_into("<H", blob, 56, self.delay_ms)
        return bytes(blob)


@dataclass
class Telemetry:
    frame_counter: int
    slider_raw: list[int]
    touch_count: list[int]
    touch_bitmap: int
    ir_raw: list[int]
    ir_blocked: list[int]
    ir_bitmap: int

    @classmethod
    def from_bytes(cls, pl: bytes) -> Telemetry:
        if len(pl) != TELEMETRY_SIZE:
            raise ValueError(f"telemetry must be {TELEMETRY_SIZE} bytes, got {len(pl)}")
        frame_counter = struct.unpack_from("<I", pl, 0)[0]
        slider_raw = [struct.unpack_from("<H", pl, 4 + i * 2)[0] for i in range(32)]
        touch_count = [struct.unpack_from("<I", pl, 68 + i * 4)[0] for i in range(32)]
        touch_bitmap = struct.unpack_from("<I", pl, 196)[0]
        ir_raw = [struct.unpack_from("<H", pl, 200 + i * 2)[0] for i in range(6)]
        ir_blocked = [pl[212 + i] for i in range(6)]
        ir_bitmap = pl[218]
        return cls(
            frame_counter=frame_counter,
            slider_raw=slider_raw,
            touch_count=touch_count,
            touch_bitmap=touch_bitmap,
            ir_raw=ir_raw,
            ir_blocked=ir_blocked,
            ir_bitmap=ir_bitmap,
        )


class ProtocolClient:
    def __init__(self, ser: serial.Serial):
        self.ser = ser
        self.rx = bytearray()
        self._seq = 0

    def _next_seq(self) -> int:
        self._seq += 1
        return self._seq

    def _drain(self, timeout: float = 0.05) -> None:
        deadline = time.time() + timeout
        while time.time() < deadline:
            chunk = self.ser.read(512)
            if not chunk:
                continue
            self.rx.extend(chunk)
            list(parse_frames(self.rx))

    def read_push_frames(self) -> list[Telemetry]:
        out: list[Telemetry] = []
        chunk = self.ser.read(512)
        if not chunk:
            return out
        self.rx.extend(chunk)
        for op, _seq, pl in parse_frames(self.rx):
            if op == OP_TELEMETRY_PUSH and len(pl) == TELEMETRY_SIZE:
                out.append(Telemetry.from_bytes(pl))
        return out

    def transact(self, opcode: int, payload: bytes = b"", timeout: float = 2.0) -> bytes:
        seq = self._next_seq()
        self.ser.write(build_frame(opcode, seq, payload))
        deadline = time.time() + timeout
        while time.time() < deadline:
            chunk = self.ser.read(512)
            if chunk:
                self.rx.extend(chunk)
                for op, s, pl in parse_frames(self.rx):
                    if op == opcode and s == seq:
                        return pl
        raise TimeoutError(f"No response for opcode 0x{opcode:02x} seq={seq}")

    def collect_frames(self, duration: float, accept_ops: set[int]) -> list[tuple[int, int, bytes]]:
        deadline = time.time() + duration
        frames: list[tuple[int, int, bytes]] = []
        while time.time() < deadline:
            chunk = self.ser.read(512)
            if chunk:
                self.rx.extend(chunk)
                for op, s, pl in parse_frames(self.rx):
                    if op in accept_ops:
                        frames.append((op, s, pl))
        return frames

    def ping(self) -> tuple[int, str, int]:
        pl = self.transact(OP_PING)
        proto_ver = pl[0]
        nul = pl.index(0, 1)
        fw = pl[1:nul].decode("ascii", errors="replace")
        board_id = struct.unpack_from("<Q", pl, nul + 1)[0]
        return proto_ver, fw, board_id

    def get_config(self) -> tuple[int, ChuConfig]:
        pl = self.transact(OP_GET_CONFIG)
        cfg_ver = pl[0]
        return cfg_ver, ChuConfig.from_bytes(pl[1:])

    def set_config(self, flags: int, cfg: ChuConfig | bytes) -> ChuConfig:
        blob = cfg if isinstance(cfg, bytes) else cfg.to_bytes()
        pl = self.transact(OP_SET_CONFIG, struct.pack("<I", flags) + blob)
        if pl[0] != STATUS_OK:
            raise RuntimeError(f"SET_CONFIG failed status={pl[0]}")
        return ChuConfig.from_bytes(pl[1:])

    def get_telemetry(self) -> Telemetry:
        pl = self.transact(OP_GET_TELEMETRY, timeout=5.0)
        if len(pl) != TELEMETRY_SIZE:
            raise RuntimeError(f"telemetry size {len(pl)}, expected {TELEMETRY_SIZE}")
        return Telemetry.from_bytes(pl)

    def capture_ir_baseline(self) -> list[int]:
        pl = self.transact(OP_CAPTURE_IR_BASE)
        if pl[0] != STATUS_OK:
            raise RuntimeError(f"CAPTURE_IR_BASELINE failed status={pl[0]}")
        return [struct.unpack_from("<H", pl, 1 + i * 2)[0] for i in range(6)]

    def stream_on(self, hz: int) -> None:
        pl = self.transact(OP_STREAM_ON, bytes([hz]))
        if pl[0] != STATUS_OK:
            raise RuntimeError(f"STREAM_ON failed status={pl[0]}")

    def stream_off(self) -> None:
        pl = self.transact(OP_STREAM_OFF)
        if pl[0] != STATUS_OK:
            raise RuntimeError(f"STREAM_OFF failed status={pl[0]}")

    def save(self) -> None:
        pl = self.transact(OP_SAVE)
        if pl[0] != STATUS_OK:
            raise RuntimeError(f"SAVE failed status={pl[0]}")

    def factory_reset(self) -> None:
        pl = self.transact(OP_FACTORY_RESET)
        if pl[0] != STATUS_OK:
            raise RuntimeError(f"FACTORY_RESET failed status={pl[0]}")

    def recalc_touch(self) -> None:
        pl = self.transact(OP_RECALC_TOUCH, timeout=10.0)
        if pl[0] != STATUS_OK:
            raise RuntimeError(f"RECALC_TOUCH failed status={pl[0]}")
