"""Main application window."""

from __future__ import annotations

from PySide6.QtCore import Qt, QThread, QTimer
from PySide6.QtGui import QIcon
from PySide6.QtWidgets import (
    QComboBox,
    QHBoxLayout,
    QLabel,
    QMainWindow,
    QMessageBox,
    QPushButton,
    QSizePolicy,
    QToolBar,
    QVBoxLayout,
    QWidget,
)

from paths import ensure_tools_on_path, resource_path  # noqa: E402

ensure_tools_on_path()

from smpl_protocol import ChuConfig, find_config_port, list_serial_ports  # noqa: E402

from config_editor import ConfigEditor  # noqa: E402
from device_worker import DeviceWorker  # noqa: E402
from telemetry_panel import TelemetryPanel  # noqa: E402

_BTN_PRIMARY = (
    "QPushButton { background: #1565c0; color: #fff; border: none;"
    " padding: 4px 14px; border-radius: 4px; font-weight: bold; }"
    "QPushButton:hover { background: #1976d2; }"
    "QPushButton:pressed { background: #0d47a1; }"
    "QPushButton:disabled { background: #1a2744; color: #4a5a6a; }"
)
_BTN_MUTED = (
    "QPushButton { background: transparent; color: #9aa0a6; border: 1px solid #3c4043;"
    " padding: 4px 10px; border-radius: 4px; }"
    "QPushButton:hover { color: #c0c6cc; border-color: #5a6066; }"
    "QPushButton:pressed { background: #1a1d20; }"
    "QPushButton:disabled { color: #555; border-color: #2a2d30; }"
)
# Segmented accent pair — left and right halves share a single border between them.
_BTN_SEG_L = (
    "QPushButton {"
    " background: transparent; color: #8ab4f8;"
    " border-top: 1px solid #4a6fa8; border-bottom: 1px solid #4a6fa8;"
    " border-left: 1px solid #4a6fa8; border-right: 0px;"
    " padding: 4px 14px;"
    " border-top-left-radius: 4px; border-bottom-left-radius: 4px;"
    " border-top-right-radius: 0px; border-bottom-right-radius: 0px; }"
    "QPushButton:hover {"
    " background: #1a2744;"
    " border-top: 1px solid #8ab4f8; border-bottom: 1px solid #8ab4f8;"
    " border-left: 1px solid #8ab4f8; border-right: 0px; }"
    "QPushButton:pressed { background: #162038; }"
    "QPushButton:disabled {"
    " color: #3a4a5a;"
    " border-top: 1px solid #2a3a4a; border-bottom: 1px solid #2a3a4a;"
    " border-left: 1px solid #2a3a4a; border-right: 0px; }"
)
_BTN_SEG_R = (
    "QPushButton {"
    " background: transparent; color: #8ab4f8;"
    " border-top: 1px solid #4a6fa8; border-bottom: 1px solid #4a6fa8;"
    " border-right: 1px solid #4a6fa8; border-left: 1px solid #2a3a4a;"
    " padding: 4px 14px;"
    " border-top-right-radius: 4px; border-bottom-right-radius: 4px;"
    " border-top-left-radius: 0px; border-bottom-left-radius: 0px; }"
    "QPushButton:hover {"
    " background: #1a2744;"
    " border-top: 1px solid #8ab4f8; border-bottom: 1px solid #8ab4f8;"
    " border-right: 1px solid #8ab4f8; border-left: 1px solid #4a6fa8; }"
    "QPushButton:pressed { background: #162038; }"
    "QPushButton:disabled {"
    " color: #3a4a5a;"
    " border-top: 1px solid #2a3a4a; border-bottom: 1px solid #2a3a4a;"
    " border-right: 1px solid #2a3a4a; border-left: 1px solid #2a3a4a; }"
)


class MainWindow(QMainWindow):
    def __init__(self) -> None:
        super().__init__()
        self.setWindowTitle("simpl-slidrr config")
        self.resize(980, 680)
        icon_path = resource_path("icons", "simpl-slidrr-config-icon.ico")
        if icon_path.is_file():
            self.setWindowIcon(QIcon(str(icon_path)))

        self._thread = QThread(self)
        self._worker = DeviceWorker()
        self._worker.moveToThread(self._thread)
        self._thread.start()

        self._connected_port: str | None = None
        self._suppressed_port: str | None = None

        self._build_toolbar()
        self._build_central()

        self._worker.connected.connect(self._on_connected)
        self._worker.disconnected.connect(self._on_disconnected)
        self._worker.telemetry.connect(self._on_telemetry)
        self._worker.config_loaded.connect(self._on_config_loaded)
        self._worker.operation_finished.connect(self._on_operation_finished)
        self._worker.error.connect(self._on_error)

        self._scan_timer = QTimer(self)
        self._scan_timer.setInterval(1500)
        self._scan_timer.timeout.connect(self._auto_scan)

        self._refresh_ports()
        self._set_connected_ui(False)

    def closeEvent(self, event) -> None:
        self._worker.disconnect_requested.emit()
        self._thread.quit()
        self._thread.wait(3000)
        super().closeEvent(event)

    @staticmethod
    def _tb_spacer(width: int) -> QWidget:
        """A fixed-width invisible spacer for breathing room between toolbar items."""
        spacer = QWidget()
        spacer.setFixedWidth(width)
        return spacer

    def _build_toolbar(self) -> None:
        tb = QToolBar("Connection")
        self.addToolBar(tb)

        # --- Left: connection controls ---
        self._port_combo = QComboBox()
        self._port_combo.setMinimumWidth(220)
        tb.addWidget(QLabel(" Port: "))
        tb.addWidget(self._port_combo)

        tb.addWidget(self._tb_spacer(8))

        self._refresh_btn = QPushButton("↻")
        self._refresh_btn.setStyleSheet(
            "QPushButton {"
            " background: transparent; color: #9aa0a6; border: 1px solid #3c4043;"
            " padding: 0px; border-radius: 4px;"
            " font-size: 15px; font-family: 'Segoe UI Symbol'; }"
            "QPushButton:hover { color: #c0c6cc; border-color: #5a6066; }"
            "QPushButton:pressed { background: #1a1d20; }"
            "QPushButton:disabled { color: #555; border-color: #2a2d30; }"
        )
        self._refresh_btn.setToolTip("Rescan serial ports")
        self._refresh_btn.clicked.connect(self._refresh_ports)
        tb.addWidget(self._refresh_btn)

        tb.addWidget(self._tb_spacer(8))

        self._connect_btn = QPushButton("Connect")
        self._connect_btn.setStyleSheet(_BTN_PRIMARY)
        self._connect_btn.clicked.connect(self._toggle_connect)
        tb.addWidget(self._connect_btn)

        # Match the refresh icon button's height to the Connect button so its
        # box doesn't stand taller than the rest of the nav, and keep it square.
        _nav_h = self._connect_btn.sizeHint().height()
        self._refresh_btn.setFixedSize(_nav_h, _nav_h)

        # --- Flexible spacer pushes config actions to the right ---
        _spacer = QWidget()
        _spacer.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Preferred)
        tb.addWidget(_spacer)

        # --- Right: Reload (read from device) ---
        self._reload_btn = QPushButton("Read config")
        self._reload_btn.setStyleSheet(_BTN_MUTED)
        self._reload_btn.setToolTip("Re-read the current config from the connected device")
        self._reload_btn.clicked.connect(lambda: self._worker.refresh_requested.emit())
        tb.addWidget(self._reload_btn)

        # --- Right: Apply + Save segmented pair (write to device) ---
        _seg = QWidget()
        _seg_row = QHBoxLayout(_seg)
        _seg_row.setContentsMargins(6, 0, 0, 0)
        _seg_row.setSpacing(0)

        self._apply_btn = QPushButton("Apply to device")
        self._apply_btn.setStyleSheet(_BTN_SEG_L)
        self._apply_btn.setToolTip("Writes changes to device RAM only. Power cycle will revert unless you also Save.")
        self._apply_btn.clicked.connect(self._apply_config)
        _seg_row.addWidget(self._apply_btn)

        self._save_btn = QPushButton("Save to flash")
        self._save_btn.setStyleSheet(_BTN_SEG_R)
        self._save_btn.setToolTip("Persists the current device config to non-volatile flash.")
        self._save_btn.clicked.connect(lambda: self._worker.save_requested.emit())
        _seg_row.addWidget(self._save_btn)

        tb.addWidget(_seg)

        self._unsaved_label = QLabel("● unsaved")
        self._unsaved_label.setStyleSheet("color: #f0a040; font-size: 11px; padding: 0 4px;")
        self._unsaved_label.setVisible(False)
        tb.addWidget(self._unsaved_label)

    def _build_branding_header(self) -> QWidget:
        header = QWidget()
        row = QHBoxLayout(header)
        row.setContentsMargins(12, 10, 12, 2)
        brand = QLabel("::simpl-slidrr")
        brand.setStyleSheet("color: #8ab4f8; font-weight: bold; font-size: 22px;")
        brand.setAlignment(Qt.AlignmentFlag.AlignCenter)
        row.addStretch()
        row.addWidget(brand)
        row.addStretch()
        return header

    def _build_central(self) -> None:
        self._telemetry_panel = TelemetryPanel()
        self._config_editor = ConfigEditor()
        self._config_editor.dirty_changed.connect(self._update_apply_enabled)
        bottom = self._config_editor.build_bottom_panel()

        self._config_editor._capture_ir_btn.clicked.connect(
            lambda: self._worker.capture_ir_requested.emit()
        )
        self._config_editor._recalc_btn.clicked.connect(
            lambda: self._worker.recalc_touch_requested.emit()
        )
        self._config_editor._factory_btn.clicked.connect(self._factory_reset)

        self._config_editor._ir_trig_slider.valueChanged.connect(
            self._telemetry_panel.set_air_trigger_pct
        )
        self._telemetry_panel.set_air_trigger_pct(self._config_editor._ir_trig_slider.value())

        self._telemetry_panel.setSizePolicy(
            QSizePolicy.Policy.Preferred, QSizePolicy.Policy.Maximum
        )
        self._connect_hint = QLabel(
            "Plug in the device via USB to connect automatically."
        )
        self._connect_hint.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self._connect_hint.setStyleSheet("color: #555; font-size: 12px; padding: 8px 0;")

        central = QWidget()
        layout = QVBoxLayout(central)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(4)
        layout.addWidget(self._build_branding_header())
        layout.addWidget(self._connect_hint)
        layout.addWidget(self._telemetry_panel, alignment=Qt.AlignmentFlag.AlignTop)
        layout.addStretch(1)
        layout.addWidget(bottom)
        layout.addWidget(self._build_footer())
        self.setCentralWidget(central)

    def _build_footer(self) -> QWidget:
        footer = QWidget()
        footer.setObjectName("statusFooter")
        footer.setStyleSheet("#statusFooter { border-top: 1px solid palette(mid); }")
        row = QHBoxLayout(footer)
        row.setContentsMargins(8, 4, 8, 4)
        self._conn_label = QLabel("Disconnected")
        self._stats_label = QLabel("Frame: —   Touches: —")
        row.addWidget(self._conn_label)
        row.addStretch()
        row.addWidget(self._stats_label)
        return footer

    def _show_status(self, message: str, timeout_ms: int = 0) -> None:
        self._conn_label.setText(message)
        if timeout_ms > 0:
            QTimer.singleShot(timeout_ms, self._restore_connection_status)

    def _restore_connection_status(self) -> None:
        if self._connected_port:
            self._conn_label.setText(f"Connected to {self._connected_port}")
        else:
            self._conn_label.setText("Disconnected")

    def _refresh_ports(self) -> None:
        current = self._port_combo.currentData()
        self._port_combo.clear()
        ports = list_serial_ports()
        auto = find_config_port()
        for device, desc in ports:
            label = f"{device} — {desc}"
            self._port_combo.addItem(label, device)
        if auto:
            idx = self._port_combo.findData(auto)
            if idx >= 0:
                self._port_combo.setCurrentIndex(idx)
        elif current:
            idx = self._port_combo.findData(current)
            if idx >= 0:
                self._port_combo.setCurrentIndex(idx)

    def _toggle_connect(self) -> None:
        if self._connect_btn.text() == "Disconnect":
            self._suppressed_port = self._connected_port
            self._worker.disconnect_requested.emit()
            return
        self._suppressed_port = None
        port = self._port_combo.currentData()
        if not port:
            QMessageBox.warning(self, "No port", "Select a serial port first.")
            return
        self._scan_timer.stop()
        self._show_status(f"Connecting to {port}…")
        self._worker.connect_requested.emit(port, 115200)

    def _auto_scan(self) -> None:
        if self._suppressed_port:
            active_ports = [p for p, _ in list_serial_ports()]
            if self._suppressed_port not in active_ports:
                self._suppressed_port = None
            else:
                return
        port = find_config_port()
        if port:
            self._scan_timer.stop()
            self._refresh_ports()
            self._show_status(f"Auto-detected {port}, connecting…")
            self._worker.connect_requested.emit(port, 115200)

    def _apply_config(self) -> None:
        cfg, flags = self._config_editor.collect_for_apply()
        if flags == 0:
            self._show_status("No config changes to apply", 3000)
            return
        self._worker.apply_requested.emit(cfg, flags)

    def _factory_reset(self) -> None:
        msg = QMessageBox(self.window())
        msg.setWindowTitle("Factory reset")
        msg.setText("Restore all settings to firmware defaults and save immediately?\nThis cannot be undone.")
        msg.setStandardButtons(QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No)
        msg.setDefaultButton(QMessageBox.StandardButton.No)
        if msg.exec() == QMessageBox.StandardButton.Yes:
            self._worker.factory_reset_requested.emit()

    def _on_telemetry(self, telem) -> None:
        self._telemetry_panel.update_telemetry(telem)
        active = bin(telem.touch_bitmap).count("1")
        self._stats_label.setText(f"Frame: {telem.frame_counter}   Touches: {active}")

    def _on_connected(self, info: dict) -> None:
        self._set_connected_ui(True)
        self._config_editor.set_device_info(
            info.get("fw", "?"),
            info.get("board_id", 0),
            info.get("cfg_ver", "?"),
        )
        self._connected_port = info.get("port")
        self._conn_label.setText(f"Connected to {self._connected_port}")

    def _on_disconnected(self) -> None:
        self._set_connected_ui(False)
        self._config_editor.clear_device_info()
        self._connected_port = None
        self._stats_label.setText("Frame: —   Touches: —")
        self._conn_label.setText("Disconnected")

    def _on_config_loaded(self, cfg: ChuConfig) -> None:
        self._config_editor.set_config(cfg)
        self._telemetry_panel.set_air_config(cfg.ir_base, cfg.ir_trigger[0])

    def _on_operation_finished(self, name: str, status: str) -> None:
        self._show_status(f"{name}: {status}", 4000)

    def _on_error(self, message: str) -> None:
        self._show_status(message, 8000)
        if not self._connected_port:
            self._scan_timer.start()

    def _set_connected_ui(self, connected: bool) -> None:
        self._connect_hint.setVisible(not connected)
        if connected:
            self._scan_timer.stop()
        else:
            self._scan_timer.start()
        self._connect_btn.setText("Disconnect" if connected else "Connect")
        self._port_combo.setEnabled(not connected)
        self._refresh_btn.setEnabled(not connected)
        for btn in (
            self._apply_btn,
            self._reload_btn,
            self._save_btn,
            self._config_editor._capture_ir_btn,
            self._config_editor._recalc_btn,
            self._config_editor._factory_btn,
        ):
            btn.setEnabled(connected)
        self._update_apply_enabled()

    def _update_apply_enabled(self, _dirty: bool | None = None) -> None:
        connected = self._connect_btn.text() == "Disconnect"
        dirty = self._config_editor.is_dirty()
        self._apply_btn.setEnabled(connected and dirty)
        self._unsaved_label.setVisible(connected and dirty)
