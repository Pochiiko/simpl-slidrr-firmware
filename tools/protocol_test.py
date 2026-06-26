#!/usr/bin/env python3
"""Smoke test for simpl-slidrr config protocol (CDC config port)."""

import argparse
import sys
import time

from smpl_protocol import (
    CFG_BLOB_SIZE,
    CFG_SECTION_TWEAK,
    OP_TELEMETRY_PUSH,
    PROTO_VERSION,
    TELEMETRY_SIZE,
    ChuConfig,
    ProtocolClient,
    find_config_port,
)

try:
    import serial
except ImportError:
    print("Install pyserial: pip install pyserial", file=sys.stderr)
    sys.exit(1)


def test_ping(cli: ProtocolClient) -> None:
    proto_ver, fw, board_id = cli.ping()
    print(f"PING: proto={proto_ver} fw={fw!r} board_id=0x{board_id:016x}")
    assert proto_ver == PROTO_VERSION


def test_get_config(cli: ProtocolClient) -> ChuConfig:
    cfg_ver, cfg = cli.get_config()
    print(f"GET_CONFIG: cfg_version={cfg_ver} blob={CFG_BLOB_SIZE} bytes")
    assert cfg_ver == 1
    return cfg


def test_get_telemetry(cli: ProtocolClient) -> None:
    telem = cli.get_telemetry()
    print(f"GET_TELEMETRY: frame={telem.frame_counter} slider_raw[0]={telem.slider_raw[0]}")


def test_set_config_roundtrip(cli: ProtocolClient, original: ChuConfig) -> None:
    orig_delay = original.delay_ms
    test_delay = 42 if orig_delay != 42 else 43

    patch = ChuConfig.from_bytes(original.to_bytes())
    patch.delay_ms = test_delay

    applied = cli.set_config(CFG_SECTION_TWEAK, patch)
    assert applied.delay_ms == test_delay

    _, read_back = cli.get_config()
    assert read_back.delay_ms == test_delay
    print(f"SET_CONFIG: delay_ms {orig_delay} -> {test_delay} (verified)")

    cli.set_config(CFG_SECTION_TWEAK, original)
    _, restored = cli.get_config()
    assert restored.delay_ms == orig_delay
    print(f"SET_CONFIG: restored delay_ms={orig_delay}")


def test_stream(cli: ProtocolClient, hz: int = 30, duration: float = 1.0) -> None:
    cli._drain()
    cli.stream_on(hz)
    frames = cli.collect_frames(duration, {OP_TELEMETRY_PUSH})
    cli.stream_off()

    expect_min = int(hz * duration * 0.6)
    expect_max = int(hz * duration * 1.4) + 2
    count = len(frames)
    print(f"STREAM_ON: {hz} Hz for {duration:.1f}s -> {count} push frames")
    if count < expect_min or count > expect_max:
        raise RuntimeError(
            f"stream frame count {count} outside [{expect_min}, {expect_max}]"
        )
    for _op, _seq, pl in frames:
        if len(pl) != TELEMETRY_SIZE:
            raise RuntimeError(f"push telemetry size {len(pl)}")


def test_capture_ir_baseline(cli: ProtocolClient) -> None:
    bases = cli.capture_ir_baseline()
    print(f"CAPTURE_IR_BASELINE: {bases}")
    assert len(bases) == 6
    assert all(0 <= v <= 4095 for v in bases)


def test_save(cli: ProtocolClient) -> None:
    cli.save()
    print("SAVE: OK")


def main():
    ap = argparse.ArgumentParser(description="simpl-slidrr protocol smoke test")
    ap.add_argument("port", nargs="?", help="Serial port (auto-detect if omitted)")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--stream-hz", type=int, default=30)
    ap.add_argument("--stream-sec", type=float, default=1.0)
    ap.add_argument("--skip-save", action="store_true", help="Skip SAVE opcode test")
    args = ap.parse_args()

    port = args.port or find_config_port()
    if not port:
        print("Config port not found. Pass COM port explicitly.", file=sys.stderr)
        sys.exit(1)

    print(f"Opening {port}")
    ser = serial.Serial(port, args.baud, timeout=0.5)
    time.sleep(0.3)

    try:
        cli = ProtocolClient(ser)
        test_ping(cli)
        original = test_get_config(cli)
        test_get_telemetry(cli)
        test_set_config_roundtrip(cli, original)
        test_capture_ir_baseline(cli)
        test_stream(cli, hz=args.stream_hz, duration=args.stream_sec)
        if not args.skip_save:
            test_save(cli)
        else:
            print("SAVE: skipped")
    finally:
        ser.close()

    print("All checks passed.")


if __name__ == "__main__":
    main()
