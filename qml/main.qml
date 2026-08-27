import QtQuick
import QtQuick.Window
import Catpaper

Window {
    id: root
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    width: Screen.width
    height: Screen.height
    visible: false

    WallpaperPicker {
        anchors.fill: parent
    }
}
