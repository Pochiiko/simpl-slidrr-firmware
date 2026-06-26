"""Config state, bottom settings panel (TASOLLER-style), and advanced dialog."""

from __future__ import annotations

from copy import deepcopy

from PySide6.QtCore import Qt, Signal
from PySide6.QtWidgets import (
    QAbstractSpinBox,
    QCheckBox,
    QComboBox,
    QDialog,
    QDialogButtonBox,
    QFormLayout,
    QGridLayout,
    QGroupBox,
    QHBoxLayout,
    QLabel,
    QPushButton,
    QSlider,
    QSpinBox,
    QVBoxLayout,
    QWidget,
)

from paths import ensure_tools_on_path  # noqa: E402

ensure_tools_on_path()

from smpl_protocol import (  # noqa: E402
    CFG_SECTION_AIME,
    CFG_SECTION_IR,
    CFG_SECTION_SENSE,
    CFG_SECTION_TWEAK,
    KEY_LABELS,
    ChuConfig,
)

from slider_layout import (  # noqa: E402
    SLIDER_COLUMNS,
    SLIDER_SENSOR_COUNT,
    column_bottom_label,
    column_top_label,
    sensor_index,
)


def _slider_row(
    label: str,
    minimum: int,
    maximum: int,
    value: int,
    suffix: str = "",
    display_offset: int = 0,
) -> tuple[QWidget, QSlider, QLabel]:
    """Label + horizontal slider + live value readout."""
    row = QWidget()
    layout = QHBoxLayout(row)
    layout.setContentsMargins(0, 0, 0, 0)
    layout.addWidget(QLabel(label))
    slider = QSlider(Qt.Orientation.Horizontal)
    slider.setRange(minimum, maximum)
    slider.setValue(value)
    val_lbl = QLabel(f"{value + display_offset}{suffix}")
    val_lbl.setMinimumWidth(36)
    val_lbl.setAlignment(Qt.AlignmentFlag.AlignRight | Qt.AlignmentFlag.AlignVCenter)

    def _sync(v: int) -> None:
        val_lbl.setText(f"{v + display_offset}{suffix}")

    slider.valueChanged.connect(_sync)
    layout.addWidget(slider, stretch=1)
    layout.addWidget(val_lbl)
    return row, slider, val_lbl


class ConfigEditor(QWidget):
    dirty_changed = Signal(bool)

    def __init__(self, parent: QWidget | None = None) -> None:
        super().__init__(parent)
        self._device_cfg = ChuConfig()
        self._dirty = False
        self._block_signals = False
        self._advanced_dialog: QDialog | None = None
        self._create_controls()

    # ------------------------------------------------------------------ UI build

    def build_bottom_panel(self) -> QWidget:
        panel = QWidget()
        row = QHBoxLayout(panel)
        row.setContentsMargins(8, 2, 8, 8)
        row.setSpacing(12)
        row.addWidget(self._build_touch_panel(), stretch=2)
        row.addWidget(self._build_system_panel(), stretch=2)
        row.addWidget(self._build_device_panel(), stretch=1)
        return panel

    def _build_touch_panel(self) -> QGroupBox:
        box = QGroupBox("Touch sensitivity")
        layout = QVBoxLayout(box)

        self._global_row, self._global_slider, _ = _slider_row(
            "Global:", 0, 18, self._device_cfg.global_sense + 9, display_offset=-9
        )
        self._global_slider.setTickPosition(QSlider.TickPosition.TicksBelow)
        self._global_slider.setTickInterval(9)
        self._global_slider.valueChanged.connect(self._on_edit)
        layout.addWidget(self._global_row)

        self._deb_touch_row, self._deb_touch_slider, _ = _slider_row(
            "Debounce touch:", 0, 7, self._device_cfg.debounce_touch
        )
        self._deb_touch_slider.valueChanged.connect(self._on_edit)
        layout.addWidget(self._deb_touch_row)

        self._deb_release_row, self._deb_release_slider, _ = _slider_row(
            "Debounce release:", 0, 7, self._device_cfg.debounce_release
        )
        self._deb_release_slider.valueChanged.connect(self._on_edit)
        layout.addWidget(self._deb_release_row)

        adv_btn = QPushButton("Per-key sensitivity and filter settings…")
        adv_btn.clicked.connect(self._open_advanced)
        layout.addWidget(adv_btn)
        layout.addStretch()
        return box

    def _build_system_panel(self) -> QGroupBox:
        box = QGroupBox("System config")
        layout = QVBoxLayout(box)

        self._delay_row, self._delay_slider, _ = _slider_row(
            "Ground slider delay:", 0, 250, self._device_cfg.delay_ms, " ms"
        )
        self._delay_slider.valueChanged.connect(self._on_edit)
        layout.addWidget(self._delay_row)

        trig = self._device_cfg.ir_trigger[0] if self._device_cfg.ir_trigger else 20
        self._ir_trig_row, self._ir_trig_slider, _ = _slider_row(
            "AIR trigger:", 1, 100, trig, " %"
        )
        self._ir_trig_slider.valueChanged.connect(self._on_edit)
        layout.addWidget(self._ir_trig_row)

        form = QFormLayout()
        self._aime_mode = QComboBox()
        self._aime_mode.addItems(["Mode 0", "Mode 1"])
        self._aime_mode.currentIndexChanged.connect(self._on_edit)
        self._aime_virtual = QCheckBox("Virtual AIC")
        self._aime_virtual.toggled.connect(self._on_edit)
        form.addRow("AIME mode", self._aime_mode)
        form.addRow(self._aime_virtual)
        layout.addLayout(form)

        hint = QLabel("Delay: 0 = no delay. Recommended 12ms")
        hint.setWordWrap(True)
        hint.setStyleSheet("color: gray; font-size: 11px;")
        layout.addWidget(hint)
        layout.addStretch()
        return box

    def _build_device_panel(self) -> QGroupBox:
        box = QGroupBox("Device")
        layout = QVBoxLayout(box)

        self._fw_label = QLabel("Firmware: —")
        self._fw_label.setWordWrap(True)
        self._board_label = QLabel("Board: —")
        self._board_label.setWordWrap(True)
        layout.addWidget(self._fw_label)
        layout.addWidget(self._board_label)
        layout.addStretch()

        btn_col = QVBoxLayout()
        self._capture_ir_btn = QPushButton("Capture IR baseline")
        self._recalc_btn = QPushButton("Recalc touch")
        self._factory_btn = QPushButton("Factory reset…")
        self._factory_btn.setStyleSheet(
            "QPushButton { color: #d9534f; border: 1px solid #6b2020; }"
            "QPushButton:hover { background: #3a1010; }"
            "QPushButton:disabled { color: #555; border-color: #444; }"
        )
        for btn in (self._capture_ir_btn, self._recalc_btn):
            btn_col.addWidget(btn)
        btn_col.addSpacing(8)
        btn_col.addWidget(self._factory_btn)
        layout.addLayout(btn_col)
        return box

    def _build_filter_group(self) -> QGroupBox:
        filt = QGroupBox("MPR121 filter")
        filt_form = QFormLayout(filt)
        filt_form.addRow("FFI (first filter iterations)", self._ffi)
        filt_form.addRow("SFI (second filter iterations)", self._sfi)
        filt_form.addRow("ESI (electrode sample interval)", self._esi)
        return filt

    def _open_advanced(self) -> None:
        if self._advanced_dialog is None:
            self._advanced_dialog = self._create_advanced_dialog()
        self._advanced_dialog.exec()

    def _create_advanced_dialog(self) -> QDialog:
        dlg = QDialog(self.window())
        dlg.setWindowTitle("Advanced settings")
        dlg.setMinimumSize(900, 380)
        root = QVBoxLayout(dlg)
        root.addWidget(self._build_filter_group())

        keys_box = QGroupBox("Per-key sensitivity (−9 … +9)")
        keys_wrap = QWidget()
        keys_grid = QGridLayout(keys_wrap)
        keys_grid.setContentsMargins(0, 0, 0, 0)
        keys_grid.setHorizontalSpacing(2)
        keys_grid.setVerticalSpacing(2)
        for row in range(2):
            for col in range(SLIDER_COLUMNS):
                idx = sensor_index(col, row)
                spin = self._key_spins[idx]
                spin.setButtonSymbols(QAbstractSpinBox.ButtonSymbols.NoButtons)
                spin.setAlignment(Qt.AlignmentFlag.AlignCenter)
                spin.setFixedWidth(50)
                keys_grid.addWidget(spin, row, col)
        keys_outer = QHBoxLayout(keys_box)
        keys_outer.addStretch()
        keys_outer.addWidget(keys_wrap)
        keys_outer.addStretch()
        root.addWidget(keys_box)

        ir_box = QGroupBox("IR baselines (read-only)")
        ir_form = QFormLayout(ir_box)
        for i, spin in enumerate(self._ir_base_spins):
            ir_form.addRow(f"IR{i + 1}", spin)
        note = QLabel("Use “Capture IR baseline” on the device panel.")
        note.setStyleSheet("color: gray;")
        note.setWordWrap(True)
        ir_form.addRow(note)
        root.addWidget(ir_box)

        buttons = QDialogButtonBox(QDialogButtonBox.StandardButton.Close)
        buttons.rejected.connect(dlg.reject)
        root.addWidget(buttons)
        return dlg

    def _create_controls(self) -> None:
        self._ffi = QSpinBox(self)
        self._ffi.setRange(0, 3)
        self._sfi = QSpinBox(self)
        self._sfi.setRange(0, 3)
        self._esi = QSpinBox(self)
        self._esi.setRange(0, 7)
        for spin in (self._ffi, self._sfi, self._esi):
            spin.valueChanged.connect(self._on_edit)

        self._key_spins: list[QSpinBox | None] = [None] * SLIDER_SENSOR_COUNT
        for row in range(2):
            for col in range(SLIDER_COLUMNS):
                idx = sensor_index(col, row)
                spin = QSpinBox(self)
                spin.setRange(-9, 9)
                spin.setToolTip(
                    f"{KEY_LABELS[idx]} ({column_top_label(col) if row == 0 else column_bottom_label(col)})"
                )
                spin.valueChanged.connect(self._on_edit)
                self._key_spins[idx] = spin

        self._ir_base_spins: list[QSpinBox] = []
        for _i in range(6):
            base = QSpinBox(self)
            base.setRange(0, 4095)
            base.setReadOnly(True)
            self._ir_base_spins.append(base)

    # ------------------------------------------------------------------ API

    def set_device_info(self, fw: str, board_id: int, cfg_ver: int) -> None:
        self._fw_label.setText(f"Firmware: {fw}")
        self._board_label.setText(f"Board: 0x{board_id:016x} · cfg v{cfg_ver}")

    def clear_device_info(self) -> None:
        self._fw_label.setText("Firmware: —")
        self._board_label.setText("Board: —")

    def device_config(self) -> ChuConfig:
        return deepcopy(self._device_cfg)

    def is_dirty(self) -> bool:
        return self._dirty

    def dirty_sections(self) -> int:
        flags = 0
        current = self._collect()
        if current.sense_fingerprint() != self._device_cfg.sense_fingerprint():
            flags |= CFG_SECTION_SENSE
        if (
            current.aime_mode != self._device_cfg.aime_mode
            or current.aime_virtual != self._device_cfg.aime_virtual
        ):
            flags |= CFG_SECTION_AIME
        if (
            current.ir_base != self._device_cfg.ir_base
            or current.ir_trigger != self._device_cfg.ir_trigger
        ):
            flags |= CFG_SECTION_IR
        if current.delay_ms != self._device_cfg.delay_ms:
            flags |= CFG_SECTION_TWEAK
        return flags

    def set_config(self, cfg: ChuConfig) -> None:
        self._device_cfg = deepcopy(cfg)
        self._block_signals = True

        self._global_slider.setValue(cfg.global_sense + 9)
        self._deb_touch_slider.setValue(cfg.debounce_touch)
        self._deb_release_slider.setValue(cfg.debounce_release)
        self._delay_slider.setValue(cfg.delay_ms)
        self._ir_trig_slider.setValue(cfg.ir_trigger[0])

        self._ffi.setValue(cfg.filter_ffi)
        self._sfi.setValue(cfg.filter_sfi)
        self._esi.setValue(cfg.filter_esi)
        for i, spin in enumerate(self._key_spins):
            if spin is not None:
                spin.setValue(cfg.keys[i])
        for i, spin in enumerate(self._ir_base_spins):
            spin.setValue(cfg.ir_base[i])

        self._aime_mode.setCurrentIndex(cfg.aime_mode)
        self._aime_virtual.setChecked(cfg.aime_virtual)

        self._block_signals = False
        self._set_dirty(False)

    def _collect(self) -> ChuConfig:
        cfg = ChuConfig.from_bytes(self._device_cfg.to_bytes())
        cfg.set_filter(self._ffi.value(), self._sfi.value(), self._esi.value())
        cfg.global_sense = self._global_slider.value() - 9
        cfg.debounce_touch = self._deb_touch_slider.value()
        cfg.debounce_release = self._deb_release_slider.value()
        cfg.keys = [spin.value() if spin is not None else 0 for spin in self._key_spins]
        cfg.ir_base = [spin.value() for spin in self._ir_base_spins]
        trig = self._ir_trig_slider.value()
        cfg.ir_trigger = [trig] * 6
        cfg.aime_mode = self._aime_mode.currentIndex()
        cfg.aime_virtual = self._aime_virtual.isChecked()
        cfg.hid_io4 = self._device_cfg.hid_io4
        cfg.delay_ms = self._delay_slider.value()
        return cfg

    def collect_for_apply(self) -> tuple[ChuConfig, int]:
        return self._collect(), self.dirty_sections()

    def _set_dirty(self, dirty: bool) -> None:
        self._dirty = dirty
        self.dirty_changed.emit(dirty)

    def _on_edit(self) -> None:
        if self._block_signals:
            return
        self._set_dirty(self.dirty_sections() != 0)
