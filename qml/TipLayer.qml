// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only
import QtQuick

Item {
    id: layer
    anchors.fill: parent
    z: 2000000
    property bool ready: false
    Timer { id: delay; interval: 450; onTriggered: layer.place() }
    Connections {
        target: TipHub
        function onOwnerChanged() {
            layer.ready = false
            if (TipHub.owner) delay.restart(); else delay.stop()
        }
    }
    function place() {
        const owner = TipHub.owner
        if (!owner)
            return
        const p = owner.mapToItem(layer, owner.width / 2, owner.height)
        bubble.x = Math.max(6, Math.min(layer.width - bubble.width - 6, p.x - bubble.width / 2))
        bubble.y = p.y + bubble.height + 12 > layer.height ? owner.mapToItem(layer, 0, 0).y - bubble.height - 6 : p.y + 6
        ready = true
    }
    Rectangle {
        id: bubble
        width: Math.min(label.implicitWidth, 360) + 20
        height: label.implicitHeight + 14
        radius: 9
        color: T.surface
        border.color: T.border
        opacity: layer.ready ? 1 : 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: T.fast; easing.type: Easing.OutCubic } }
        Shadow { blur: 10; offset: 2 }
        Text {
            id: label
            x: 10; y: 7
            width: Math.min(implicitWidth, 360)
            text: TipHub.text
            color: T.text
            font.pixelSize: T.callout
            wrapMode: Text.Wrap
        }
    }
}
