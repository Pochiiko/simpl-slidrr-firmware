"""Background serial I/O worker (runs on a QThread)."""

from __future__ import annotations

import time

from PySide6.QtCore import QObject, QTimer, Signal, Slot

from paths import ensure_tools_on_path  # noqa: E402

ensure_tools_on_path()

from smpl_protocol import (  # noqa: E402
    ChuConfig,
    ProtocolClient,
    Telemetry,
)

try:
    import serial
    from serial import SerialException
except ImportError:
    serial = None  # type: ignore[assignment]
    SerialException = OSError  # type: ignore[misc, assignment]


class DeviceWorker(QObject):
    connect_requested = Signal(str, int)
    disconnect_requested = Signal()
    refresh_requested = Signal()
    apply_requested = Signal(object, int)
    save_requested = Signal()
    factory_reset_requested = Signal()
    recalc_touch_requested = Signal()
    capture_ir_requested = Signal()

    connected = Signal(dict)
    disconnected = Signal()
    telemetry = Signal(object)
    config_loaded = Signal(object)
    operation_finished = Signal(str, str)
    error = Signal(str)

    def __init__(self) -> None:
        super().__init__()
        self._ser = None
        self._cli: ProtocolClient | None = None
        self._poll = QTimer(self)
        self._poll.setInterval(16)
        self._poll.timeout.connect(self._poll_serial)
        self.connect_requested.connect(self.connect_port)
        self.disconnect_requested.connect(self.disconnect_port)
        self.refresh_requested.connect(self.refresh_config)
        self.apply_requested.connect(self.apply_config)
        self.save_requested.connect(self.save_config)
        self.factory_reset_requested.connect(self.factory_reset)
        self.recalc_touch_requested.connect(self.recalc_touch)
        self.capture_ir_requested.connect(self.capture_ir_baseline)

    def _is_link_lost(self, exc: BaseException) -> bool:
        if isinstance(exc, (SerialException, OSError, PermissionError)):
            return True
        if self._ser is not None and not self._ser.is_open:
            return True
        msg = str(exc).lower()
        return any(
            token in msg
            for token in (
                "device not configured",
                "access is denied",
                "semaphore",
                "port not open",
                "broken pipe",
                "disconnected",
                "connection",
            )
        )

    def _handle_link_lost(self) -> None:
        if self._cli is None and self._ser is None:
            return
        self._disconnect_internal()
        self.disconnected.emit()

    @Slot(str, int)
    def connect_port(self, port: str, baud: int = 115200) -> None:
        if serial is None:
            self.error.emit("pyserial is not installed")
            return
        self._disconnect_internal()
        try:
            self._ser = serial.Serial(port, baud, timeout=0.05)
            time.sleep(0.3)
            self._cli = ProtocolClient(self._ser)
            proto_ver, fw, board_id = self._cli.ping()
            self._cli.stream_on(60)
            cfg_ver, cfg = self._cli.get_config()
            self.config_loaded.emit(cfg)
            self.connected.emit(
                {
                    "port": port,
                    "proto_ver": proto_ver,
                    "fw": fw,
                    "board_id": board_id,
                    "cfg_ver": cfg_ver,
                }
            )
            self._poll.start()
        except Exception as exc:
            self._disconnect_internal()
            if not self._is_link_lost(exc):
                self.error.emit(str(exc))

    @Slot()
    def disconnect_port(self) -> None:
        self._disconnect_internal()
        self.disconnected.emit()

    @Slot()
    def refresh_config(self) -> None:
        self._run_op("refresh_config", self._do_refresh_config)

    @Slot(object, int)
    def apply_config(self, cfg: ChuConfig, flags: int) -> None:
        self._run_op("apply_config", lambda: self._do_apply_config(cfg, flags))

    @Slot()
    def save_config(self) -> None:
        self._run_op("save", lambda: self._cli.save())

    @Slot()
    def factory_reset(self) -> None:
        self._run_op("factory_reset", self._do_factory_reset)

    @Slot()
    def recalc_touch(self) -> None:
        self._run_op("recalc_touch", lambda: self._cli.recalc_touch())

    @Slot()
    def capture_ir_baseline(self) -> None:
        self._run_op("capture_ir_baseline", self._do_capture_ir_baseline)

    def _run_op(self, name: str, fn) -> None:
        if not self._cli:
            self.error.emit("Not connected")
            return
        was_polling = self._poll.isActive()
        if was_polling:
            self._poll.stop()
        try:
            fn()
            self.operation_finished.emit(name, "ok")
        except Exception as exc:
            if self._is_link_lost(exc):
                self._handle_link_lost()
            else:
                self.error.emit(f"{name}: {exc}")
        finally:
            if self._cli and was_polling:
                self._poll.start()

    def _do_refresh_config(self) -> None:
        _, cfg = self._cli.get_config()
        self.config_loaded.emit(cfg)

    def _do_apply_config(self, cfg: ChuConfig, flags: int) -> None:
        applied = self._cli.set_config(flags, cfg)
        self.config_loaded.emit(applied)

    def _do_factory_reset(self) -> None:
        self._cli.factory_reset()
        _, cfg = self._cli.get_config()
        self.config_loaded.emit(cfg)

    def _do_capture_ir_baseline(self) -> None:
        bases = self._cli.capture_ir_baseline()
        _, cfg = self._cli.get_config()
        for i, v in enumerate(bases):
            cfg.ir_base[i] = v
        self.config_loaded.emit(cfg)

    @Slot()
    def _poll_serial(self) -> None:
        if not self._cli:
            return
        try:
            for telem in self._cli.read_push_frames():
                self.telemetry.emit(telem)
        except Exception as exc:
            if self._is_link_lost(exc):
                self._handle_link_lost()
            else:
                self.error.emit(f"read: {exc}")

    def _disconnect_internal(self) -> None:
        self._poll.stop()
        if self._cli:
            try:
                self._cli.stream_off()
            except Exception:
                pass
        self._cli = None
        if self._ser and self._ser.is_open:
            self._ser.close()
        self._ser = None
