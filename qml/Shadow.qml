// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only
import QtQuick

// A soft shadow behind the parent, drawn as a few rounded shapes: a blur would need an
// offscreen pass for every panel.
Item {
    id: root
    property int blur: 16
    property int offset: 4
    readonly property real corner: parent && parent.radius !== undefined ? parent.radius : 0
    anchors.fill: parent
    z: -1
    Repeater {
        model: 7
        Rectangle {
            required property int index
            readonly property real spread: root.blur * (index + 1) / 7
            x: -spread; y: -spread + root.offset
            width: root.width + spread * 2
            height: root.height + spread * 2
            radius: root.corner + spread
            color: Qt.rgba(T.shadow.r, T.shadow.g, T.shadow.b, T.shadow.a / 6)
        }
    }
}
