from PySide6.QtCore import QObject, QSettings, Property, Signal


class PersistentSettings(QObject):
    queryChanged = Signal()
    searchedChanged = Signal()
    lastNameChanged = Signal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self._qs = QSettings("catpaper", "WallpaperPicker")
        self._query = str(self._qs.value("query", ""))
        self._searched = self._qs.value("searched", "false") in ("true", True)
        self._lastName = str(self._qs.value("lastName", ""))

    query = Property(str, lambda self: self._query, notify=queryChanged)
    searched = Property(bool, lambda self: self._searched, notify=searchedChanged)
    lastName = Property(str, lambda self: self._lastName, notify=lastNameChanged)

    @query.setter
    def query(self, v):
        if v != self._query:
            self._query = v
            self._qs.setValue("query", v)
            self.queryChanged.emit()

    @searched.setter
    def searched(self, v):
        if v != self._searched:
            self._searched = v
            self._qs.setValue("searched", v)
            self.searchedChanged.emit()

    @lastName.setter
    def lastName(self, v):
        if v != self._lastName:
            self._lastName = v
            self._qs.setValue("lastName", v)
            self.lastNameChanged.emit()
