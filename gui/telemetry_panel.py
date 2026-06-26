"""Live telemetry display — TASOLLER-style air strings + slider tiles."""

from __future__ import annotations

from PySide6.QtCore import Qt
from PySide6.QtWidgets import (
    QFrame,
    QGridLayout,
    QHBoxLayout,
    QLabel,
    QSizePolicy,
    QVBoxLayout,
    QWidget,
)

from paths import ensure_tools_on_path  # noqa: E402

ensure_tools_on_path()

from smpl_protocol import KEY_LABELS, Telemetry  # noqa: E402
from slider_layout import (  # noqa: E402
    SLIDER_COLUMNS,
    SLIDER_SENSOR_COUNT,
    column_bottom_label,
    column_top_label,
    sensor_index,
)

_TILE_W = 50
_TILE_H = 90
_KEY_GAP = 2
_TILE_IDLE = "#2b2f36"
_TILE_ACTIVE = "#1b5e20"
_AIR_TRACK = "#1a2744"
_AIR_IDLE = "#2a5080"
_AIR_ACTIVE = "#4fc3f7"


class _AirStringBar(QWidget):
    """One horizontal air-sensor strip (beam status)."""

    def __init__(self, sensor_index: int, parent: QWidget | None = None) -> None:
        super().__init__(parent)
        self.sensor_index = sensor_index
        row = QHBoxLayout(self)
        row.setContentsMargins(0, 3, 0, 3)
        row.setSpacing(8)

        num = QLabel(str(sensor_index + 1))
        num.setFixedWidth(16)
        num.setAlignment(Qt.AlignmentFlag.AlignRight | Qt.AlignmentFlag.AlignVCenter)
        num.setStyleSheet("color: #8ab4f8; font-weight: bold;")

        self._beam = QFrame()
        self._beam.setFixedHeight(12)
        self._beam.setSizePolicy(QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Fixed)
        self._beam.setFrameShape(QFrame.Shape.NoFrame)

        self._raw = QLabel("—")
        self._raw.setFixedWidth(44)
        self._raw.setAlignment(Qt.AlignmentFlag.AlignRight | Qt.AlignmentFlag.AlignVCenter)
        self._raw.setStyleSheet("color: #9aa0a6; font-size: 11px;")

        row.addWidget(num)
        row.addWidget(self._beam, stretch=1)
        row.addWidget(self._raw)

    def set_state(self, raw: int, blocked: bool) -> None:
        self._raw.setText(str(raw))
        if blocked:
            self._beam.setStyleSheet(
                f"background-color: {_AIR_ACTIVE}; border-radius: 4px; border: 1px solid #81d4fa;"
            )
        else:
            self._beam.setStyleSheet(
                f"background-color: {_AIR_IDLE}; border-radius: 4px; border: 1px solid {_AIR_TRACK};"
            )


class _KeyTile(QFrame):
    """Portrait slider electrode tile."""

    def __init__(self, parent: QWidget | None = None) -> None:
        super().__init__(parent)
        self.setFixedSize(_TILE_W, _TILE_H)
        self.setFrameShape(QFrame.Shape.NoFrame)

        layout = QVBoxLayout(self)
        layout.setContentsMargins(4, 4, 4, 4)

        self._raw = QLabel("—")
        self._raw.setAlignment(Qt.AlignmentFlag.AlignCenter)
        layout.addStretch()
        layout.addWidget(self._raw)
        layout.addStretch()
        self._apply_idle()

    def _apply_idle(self) -> None:
        self.setStyleSheet(
            f"QFrame {{ background-color: {_TILE_IDLE}; border: none; border-radius: 5px; }}"
        )
        self._raw.setStyleSheet("font-size: 18px; font-weight: bold; color: #e8eaed;")

    def _apply_active(self) -> None:
        self.setStyleSheet(
            f"QFrame {{ background-color: {_TILE_ACTIVE}; border: none; border-radius: 5px; }}"
        )
        self._raw.setStyleSheet("font-size: 18px; font-weight: bold; color: #ffffff;")

    def set_state(self, raw: int, touched: bool) -> None:
        self._raw.setText(str(raw))
        if touched:
            self._apply_active()
        else:
            self._apply_idle()


class TelemetryPanel(QWidget):
    def __init__(self, parent: QWidget | None = None) -> None:
        super().__init__(parent)
        root = QVBoxLayout(self)
        root.setContentsMargins(12, 8, 12, 4)
        root.setSpacing(8)

        # --- Air strings (TASOLLER "AIR" block) ---
        air_wrap = QWidget()
        air_outer = QHBoxLayout(air_wrap)
        air_outer.setContentsMargins(0, 0, 0, 0)
        air_outer.setSpacing(10)

        air_title = QLabel("AIR:")
        air_title.setAlignment(Qt.AlignmentFlag.AlignTop)
        air_title.setStyleSheet("color: #8ab4f8; font-weight: bold; font-size: 13px; padding-top: 6px;")
        air_outer.addWidget(air_title)

        air_col = QVBoxLayout()
        air_col.setSpacing(2)
        self._air_bars: list[_AirStringBar] = []
        for sensor_i in range(5, -1, -1):
            bar = _AirStringBar(sensor_i)
            self._air_bars.append(bar)
            air_col.addWidget(bar)
        air_outer.addLayout(air_col, stretch=1)
        root.addWidget(air_wrap)

        # --- Slider keys (two tight portrait rows) ---
        keys_wrap = QWidget()
        keys_wrap.setSizePolicy(QSizePolicy.Policy.Fixed, QSizePolicy.Policy.Fixed)
        keys_grid = QGridLayout(keys_wrap)
        keys_grid.setContentsMargins(0, 4, 0, 0)
        keys_grid.setHorizontalSpacing(_KEY_GAP)
        keys_grid.setVerticalSpacing(_KEY_GAP)

        self._cells: list[_KeyTile | None] = [None] * SLIDER_SENSOR_COUNT
        for row in range(2):
            for col in range(SLIDER_COLUMNS):
                idx = sensor_index(col, row)
                tile = _KeyTile()
                tile.setToolTip(
                    f"{KEY_LABELS[idx]} ({column_top_label(col) if row == 0 else column_bottom_label(col)})"
                )
                self._cells[idx] = tile
                keys_grid.addWidget(tile, row, col)

        root.addWidget(keys_wrap, alignment=Qt.AlignmentFlag.AlignTop | Qt.AlignmentFlag.AlignHCenter)

    def update_telemetry(self, telem: Telemetry) -> None:
        for i, cell in enumerate(self._cells):
            if cell is None:
                continue
            touched = bool(telem.touch_bitmap & (1 << i))
            cell.set_state(telem.slider_raw[i], touched)

        for bar in self._air_bars:
            # Labels run 6→1 top-to-bottom; firmware index order is the inverse.
            i = 5 - bar.sensor_index
            bar.set_state(telem.ir_raw[i], bool(telem.ir_blocked[i]))
