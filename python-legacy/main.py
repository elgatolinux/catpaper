import os
import sys

from PySide6.QtCore import QTimer, QUrl
from PySide6.QtGui import QColor
from PySide6.QtQml import QQmlApplicationEngine, qmlRegisterType
from PySide6.QtWidgets import QApplication

from backend import Backend, Paths
from palette import PywalTheme
from pwsettings import PersistentSettings

TYPES = [(Paths, "Paths"), (PywalTheme, "PywalTheme"),
         (Backend, "Backend"), (PersistentSettings, "PersistentSettings")]


def main():
    app = QApplication(sys.argv)
    app.setApplicationName("catpaper")
    app.setOrganizationName("catpaper")

    for cls, name in TYPES:
        qmlRegisterType(cls, "Catpaper", 1, 0, name)

    engine = QQmlApplicationEngine()
    qml_file = os.path.join(os.path.dirname(os.path.abspath(__file__)), "main.qml")
    engine.load(QUrl.fromLocalFile(qml_file))

    roots = engine.rootObjects()
    if not roots:
        print("ERROR: no se pudo cargar la UI (revisa los errores QML arriba)", file=sys.stderr)
        sys.exit(1)

    root = roots[0]
    try:
        root.setColor(QColor(0, 0, 0, 0))
    except Exception:
        pass

    if os.environ.get("CATPAPER_SMOKE"):
        QTimer.singleShot(5000, app.quit)

    sys.exit(app.exec())


if __name__ == "__main__":
    main()
