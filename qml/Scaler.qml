import QtQuick

QtObject {
    property real defaultWidth: 2560
    property real currentWidth: defaultWidth

    function s(val) {
        return (val * currentWidth) / defaultWidth
    }
}
