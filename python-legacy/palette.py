import json
import os
from pathlib import Path

from PySide6.QtCore import QFileSystemWatcher, QObject, Property, Signal
from PySide6.QtGui import QColor

PALETTE = Path(os.path.expanduser("~/.cache/wal/colors.json"))


class PywalTheme(QObject):
    themeChanged = Signal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self._base = QColor("#0c0c1e")
        self._foreground = QColor("#c2c2c6")
        self._colors = []
        self._load()
        self._watcher = QFileSystemWatcher()
        if PALETTE.exists():
            self._watcher.addPath(str(PALETTE))
        self._watcher.fileChanged.connect(self._on_changed)

    def _on_changed(self):
        self._watcher.addPath(str(PALETTE))
        self._load()
        self.themeChanged.emit()

    def _load(self):
        try:
            data = json.loads(PALETTE.read_text(errors="ignore"))
        except Exception:
            return
        special = data.get("special") or {}
        colors = data.get("colors") or {}
        bg_src = data.get("background") or special.get("background") or "#0c0c1e"
        fg_src = data.get("foreground") or special.get("foreground") or "#c2c2c6"
        bg = QColor(str(bg_src))
        fg = QColor(str(fg_src))
        if not bg.isValid():
            bg = QColor("#0c0c1e")
        if not fg.isValid():
            fg = QColor("#c2c2c6")
        self._base = bg
        self._foreground = fg
        self._colors = []
        for i in range(16):
            src = data.get("color%d" % i) or colors.get("color%d" % i) or ""
            c = QColor(str(src))
            self._colors.append(c if c.isValid() else bg)

    @staticmethod
    def _blend(base, top, p):
        r = base.red() + (top.red() - base.red()) * p
        g = base.green() + (top.green() - base.green()) * p
        b = base.blue() + (top.blue() - base.blue()) * p
        return QColor(int(r), int(g), int(b))

    @staticmethod
    def _darken(c, factor):
        return QColor(int(c.red() * factor), int(c.green() * factor), int(c.blue() * factor))

    @property
    def _surface1(self):
        return self._blend(self._base, self._foreground, 0.12)

    @property
    def _surface2(self):
        return self._blend(self._base, self._foreground, 0.18)

    @property
    def _surface0(self):
        return self._blend(self._base, self._foreground, 0.07)

    @property
    def _mantle(self):
        return self._darken(self._base, 0.8)

    base = Property(QColor, lambda self: self._base, notify=themeChanged)
    text = Property(QColor, lambda self: self._foreground, notify=themeChanged)
    textDim = Property(QColor,
                       lambda self: QColor(self._foreground.red(), self._foreground.green(),
                                           self._foreground.blue(), 170),
                       notify=themeChanged)
    mantle = Property(QColor, lambda self: self._mantle, notify=themeChanged)
    surface0 = Property(QColor, lambda self: self._surface0, notify=themeChanged)
    surface1 = Property(QColor, lambda self: self._surface1, notify=themeChanged)
    surface2 = Property(QColor, lambda self: self._surface2, notify=themeChanged)
