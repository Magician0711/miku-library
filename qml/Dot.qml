// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only
import QtQuick

// A status shown by shape as well as colour: filled for fine, a ring for nothing found,
// a cross for failed, and a pulse while waiting.
Item {
    id: dot
    property string status: "busy"
    readonly property color ink: T.dot(status)
    width: 9; height: 9
    Rectangle {
        anchors.centerIn: parent
        width: 8; height: 8; radius: 4
        visible: dot.status !== "err"
        color: dot.status === "warn" ? "transparent" : dot.ink
        border.width: dot.status === "warn" ? 1.5 : 0
        border.color: dot.ink
        SequentialAnimation on opacity {
            running: dot.status === "busy" && Theme.animated
            loops: Animation.Infinite
            alwaysRunToEnd: true
            NumberAnimation { to: 0.35; duration: 600; easing.type: Easing.InOutSine }
            NumberAnimation { to: 1; duration: 600; easing.type: Easing.InOutSine }
        }
    }
    Repeater {
        model: dot.status === "err" ? [45, -45] : []
        Rectangle {
            required property int modelData
            anchors.centerIn: parent
            width: 10; height: 1.8; radius: 1
            rotation: modelData
            color: dot.ink
        }
    }
}
