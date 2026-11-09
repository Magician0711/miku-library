// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only
import QtQuick

// A short notice over the window that fades away by itself.
Rectangle {
    id: toast
    width: row.implicitWidth + 36
    height: row.implicitHeight + 24
    radius: height / 2
    color: T.surface
    border.color: T.border
    opacity: 0
    visible: opacity > 0
    z: 20
    Shadow { blur: 18; offset: 5 }
    property string iconName: "check"
    function show(text, icon) {
        label.text = text
        iconName = icon
        leave.stop()
        hold.stop()
        enter.restart()
        hold.restart()
    }
    NumberAnimation { id: enter; target: toast; property: "opacity"; to: 1; duration: T.fast; easing.type: Easing.OutCubic }
    NumberAnimation { id: leave; target: toast; property: "opacity"; to: 0; duration: T.slow; easing.type: Easing.OutCubic }
    Timer { id: hold; interval: 2400; onTriggered: leave.start() }
    Row {
        id: row
        anchors.centerIn: parent
        spacing: 9
        Icon { anchors.verticalCenter: parent.verticalCenter; size: 17; name: toast.iconName; tint: T.accentText }
        Text { id: label; anchors.verticalCenter: parent.verticalCenter; color: T.text; font.pixelSize: T.body }
    }
}
