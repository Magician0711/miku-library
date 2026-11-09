// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Effects

// The floating material: whatever sits behind it, blurred and lightened, under a thin
// tint and a hairline highlight. Reserved for the sidebar. With Windows transparency
// effects off it is an opaque panel.
Item {
    id: glass
    required property Item backdrop
    property real radius: 16
    // Widest this gets. The blur keeps one texture size, so that a width animation does
    // not reallocate it on every frame.
    property real fullWidth: width
    readonly property bool live: Theme.transparency

    Rectangle {
        anchors.fill: parent
        radius: glass.radius
        color: T.glassSolid
        visible: !glass.live
    }
    // Under the blur, so that where no content lies behind the panel the page colour shows.
    Rectangle {
        anchors.fill: parent
        radius: glass.radius
        color: T.bg
        visible: glass.live
    }
    ShaderEffectSource {
        id: behind
        anchors.fill: parent
        visible: false
        sourceItem: glass.live ? glass.backdrop : null
        sourceRect: Qt.rect(glass.x - glass.backdrop.x, glass.y, glass.width, glass.height)
        // Blurred anyway: a quarter of the resolution costs a sixteenth of the work.
        textureSize: Qt.size(Math.ceil(glass.fullWidth / 3), Math.ceil(glass.height / 3))
        live: true
        recursive: false
    }
    MultiEffect {
        id: blurred
        anchors.fill: parent
        visible: glass.live
        source: behind
        blurEnabled: true
        blurMax: 32
        blur: 1
        saturation: 0.35
        maskEnabled: true
        maskSource: shape
        autoPaddingEnabled: false
    }
    Item {
        id: shape
        anchors.fill: parent
        visible: false
        layer.enabled: true
        Rectangle { anchors.fill: parent; radius: glass.radius; color: "black" }
    }
    Rectangle {
        anchors.fill: parent
        radius: glass.radius
        color: glass.live ? T.glass : "transparent"
        border.width: 1
        border.color: T.glassEdge
    }
}
