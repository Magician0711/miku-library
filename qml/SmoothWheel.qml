// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only
import QtQuick

// Mouse-wheel scrolling for a Flickable with interactive set to false: each notch eases
// towards its target, and further notches extend the same movement.
Item {
    id: root
    required property Flickable view
    // Pointer handlers only see events inside their own item, so this item has to cover
    // the view. Declared inside a Flickable it would otherwise be a zero-sized child of
    // the moving content.
    parent: view
    anchors.fill: parent
    property real destination: 0
    // True while the view rests at its top, so growing content can keep it there.
    property bool pinned: true
    NumberAnimation {
        id: glide
        target: root.view
        property: "contentY"
        duration: Theme.animated ? 180 : 0
        easing.type: Easing.OutCubic
    }
    function scrollBy(delta) {
        const top = view.originY - view.topMargin
        const bottom = Math.max(top, view.originY + view.contentHeight + view.bottomMargin - view.height)
        const from = glide.running ? destination : view.contentY
        destination = Math.max(top, Math.min(bottom, from + delta))
        pinned = destination <= top + 1
        glide.stop()
        glide.to = destination
        glide.start()
    }
    WheelHandler {
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        onWheel: event => {
            const pixels = event.pixelDelta.y !== 0 ? event.pixelDelta.y : event.angleDelta.y / 120 * 110
            root.scrollBy(-pixels)
            event.accepted = true
        }
    }
}
