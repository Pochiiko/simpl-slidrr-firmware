#!/usr/bin/env python3
"""simpl-slidrr desktop config tool (Qt / PySide6)."""

import sys

from PySide6.QtWidgets import QApplication

from main_window import MainWindow


def main() -> int:
    app = QApplication(sys.argv)
    app.setApplicationName("simpl-slidrr config")
    window = MainWindow()
    window.show()
    return app.exec()


if __name__ == "__main__":
    raise SystemExit(main())
