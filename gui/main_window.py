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

        self._build_toolbar()
        self._build_central()

        self._worker.connected.connect(self._on_connected)
        self._worker.disconnected.connect(self._on_disconnected)
        self._worker.telemetry.connect(self._on_telemetry)
        self._worker.config_loaded.connect(self._on_config_loaded)
        self._worker.operation_finished.connect(self._on_operation_finished)
        self._worker.error.connect(self._on_error)

        self._refresh_ports()
        self._set_connected_ui(False)

    def closeEvent(self, event) -> None:
        self._worker.disconnect_requested.emit()
        self._thread.quit()
        self._thread.wait(3000)
        super().closeEvent(event)

    def _build_toolbar(self) -> None:
        tb = QToolBar("Connection")
        self.addToolBar(tb)

        self._port_combo = QComboBox()
        self._port_combo.setMinimumWidth(220)
        tb.addWidget(QLabel(" Port: "))
        tb.addWidget(self._port_combo)

        self._refresh_btn = QPushButton("Refresh")
        self._refresh_btn.clicked.connect(self._refresh_ports)
        tb.addWidget(self._refresh_btn)

        self._connect_btn = QPushButton("Connect")
        self._connect_btn.clicked.connect(self._toggle_connect)
        tb.addWidget(self._connect_btn)

        tb.addSeparator()
        self._apply_btn = QPushButton("Apply")
        self._apply_btn.setToolTip("Send dirty config sections to device (RAM; auto-save ~5s)")
        self._apply_btn.clicked.connect(self._apply_config)
        tb.addWidget(self._apply_btn)

        self._reload_btn = QPushButton("Reload")
        self._reload_btn.clicked.connect(lambda: self._worker.refresh_requested.emit())
        tb.addWidget(self._reload_btn)

        self._save_btn = QPushButton("Save")
        self._save_btn.clicked.connect(lambda: self._worker.save_requested.emit())
        tb.addWidget(self._save_btn)

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

        self._telemetry_panel.setSizePolicy(
            QSizePolicy.Policy.Preferred, QSizePolicy.Policy.Maximum
        )
        central = QWidget()
        layout = QVBoxLayout(central)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(4)
        layout.addWidget(self._build_branding_header())
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
            self._worker.disconnect_requested.emit()
            return
        port = self._port_combo.currentData()
        if not port:
            QMessageBox.warning(self, "No port", "Select a serial port first.")
            return
        self._show_status(f"Connecting to {port}…")
        self._worker.connect_requested.emit(port, 115200)

    def _apply_config(self) -> None:
        cfg, flags = self._config_editor.collect_for_apply()
        if flags == 0:
            self._show_status("No config changes to apply", 3000)
            return
        self._worker.apply_requested.emit(cfg, flags)

    def _factory_reset(self) -> None:
        answer = QMessageBox.question(
            self,
            "Factory reset",
            "Restore all settings to firmware defaults and save immediately?",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
        )
        if answer == QMessageBox.StandardButton.Yes:
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

    def _on_operation_finished(self, name: str, status: str) -> None:
        self._show_status(f"{name}: {status}", 4000)

    def _on_error(self, message: str) -> None:
        self._show_status(message, 8000)

    def _set_connected_ui(self, connected: bool) -> None:
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
