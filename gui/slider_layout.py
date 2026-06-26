"""Physical Chu slider key layout (16 paired columns, 32 sensors).

Firmware order is 1A, 1B, 2A, 2B, … but on the panel each column is::

    1  3  5  7  …  31   (A rail, top row)
    2  4  6  8  …  32   (B rail, bottom row)
"""

SLIDER_COLUMNS = 16
SLIDER_SENSOR_COUNT = 32


def sensor_index(col: int, row: int) -> int:
    """Map grid (col, row) to firmware sensor index 0..31."""
    return col * 2 + row


def column_top_label(col: int) -> str:
    return str(col * 2 + 1)


def column_bottom_label(col: int) -> str:
    return str(col * 2 + 2)
