"""Resolve repo paths for dev runs and PyInstaller bundles."""

from __future__ import annotations

import sys
from pathlib import Path


def tools_dir() -> Path:
    if getattr(sys, "frozen", False):
        return Path(sys._MEIPASS) / "tools"
    return Path(__file__).resolve().parent.parent / "tools"


def resource_path(*parts: str) -> Path:
    """Path to a file shipped next to gui sources (icons, etc.)."""
    if getattr(sys, "frozen", False):
        base = Path(sys._MEIPASS)
    else:
        base = Path(__file__).resolve().parent
    return base.joinpath(*parts)


def ensure_tools_on_path() -> None:
    directory = str(tools_dir())
    if directory not in sys.path:
        sys.path.insert(0, directory)
