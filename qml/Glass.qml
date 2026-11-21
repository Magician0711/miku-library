// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Effects

// Liquid Glass, for the floating layer only: the navigation bar and the window controls.
// What lies behind is blurred and its colour lifted; along the rim the glass bends the
// content just past its edge into view, the way a thick lens does; light catches the top
// edge and the foot gathers a faint shade. It has no colour of its own.
//
// With "Transparency effects" off in Windows it is an opaque panel.
Item {
    id: glass
    required property Item backdrop
    property real radius: height / 2
    // How much the rim bends: the width of the band, and how far past the edge it reaches.
    property real lens: Math.min(14, height / 3)
    property bool shadow: true
    // false: the window background's own material, laid over the content: the same heavy,
    // nearly colourless blur and the same tint, set apart only by a faint veil and a
    // hairline. No bending rim, sheen or highlight.
    property bool liquid: true
    readonly property bool live: Theme.transparency && !Theme.highContrast
    // Where the glass sits over the backdrop: its own position, carried from its parent's
    // coordinates into the backdrop's. The position has to be used in the expression for
    // the binding to follow it: reading it only to name it (as a dropped expression or an
    // unused argument) was optimised away by the compiler, and the glass kept blurring
    // whatever lay where it had been at start-up until something else moved it.
    readonly property point at: backdrop && parent ? backdrop.mapFromItem(parent, x, y) : Qt.point(0, 0)

    // A soft shadow made of rings outside the edge: one under the glass would show through it.
    Repeater {
        model: glass.shadow ? 6 : 0
        Rectangle {
            required property int index
            readonly property real spread: 2 + index * 3
            x: -spread; y: -spread + 4
            z: -1
            width: glass.width + spread * 2
            height: glass.height + spread * 2
            radius: glass.radius + spread
            color: "transparent"
            border.width: 3
            border.color: Qt.rgba(T.shadow.r, T.shadow.g, T.shadow.b, T.shadow.a * (0.55 - index * 0.08))
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: glass.radius
        color: T.glassSolid
        visible: !glass.live
    }
    // Over an opaque window, the page colour where nothing lies behind; over a see-through
    // one the system's blur of the desktop is already there.
    Rectangle {
        anchors.fill: parent
        radius: glass.radius
        color: T.bg
        visible: glass.live && !Chrome.translucent
    }

    Item {
        id: shape
        anchors.fill: parent
        visible: false
        layer.enabled: true
        Rectangle { anchors.fill: parent; radius: glass.radius; color: "black" }
    }
    ShaderEffectSource {
        id: behind
        objectName: "glassBehind"
        anchors.fill: parent
        visible: false
        sourceItem: glass.live ? glass.backdrop : null
        sourceRect: Qt.rect(glass.at.x, glass.at.y, glass.width, glass.height)
        // Blurred anyway: a third of the resolution is plenty.
        textureSize: Qt.size(Math.max(1, Math.ceil(glass.width / 3)), Math.max(1, Math.ceil(glass.height / 3)))
        live: true
        recursive: false
    }
    MultiEffect {
        objectName: "glassBlur"
        anchors.fill: parent
        visible: glass.live
        source: behind
        blurEnabled: true
        // The plain panel matches the system's background blur, which melts what lies
        // behind completely; the liquid glass keeps some of the shapes.
        blurMax: glass.liquid ? 40 : 64
        blurMultiplier: glass.liquid ? 0 : 1.5
        blur: 1
        saturation: glass.liquid ? 0.5 : 0.15
        brightness: glass.liquid ? (T.dark ? -0.03 : 0.05) : 0
        maskEnabled: true
        maskThresholdMin: 0.5
        maskSpreadAtMin: 1
        maskSource: shape
        autoPaddingEnabled: false
    }

    // The lens: the rim shows the content from a little beyond the edge, squeezed in.
    ShaderEffectSource {
        id: beyond
        anchors.fill: parent
        visible: false
        sourceItem: glass.live && glass.liquid ? glass.backdrop : null
        sourceRect: Qt.rect(glass.at.x - glass.lens, glass.at.y - glass.lens, glass.width + 2 * glass.lens, glass.height + 2 * glass.lens)
        textureSize: Qt.size(Math.max(1, Math.ceil(glass.width / 2)), Math.max(1, Math.ceil(glass.height / 2)))
        live: true
        recursive: false
    }
    Item {
        id: rim
        anchors.fill: parent
        visible: false
        layer.enabled: true
        // A band along the edge whose inner side fades out.
        Rectangle {
            id: band
            anchors.fill: parent
            radius: glass.radius
            color: "transparent"
            border.width: glass.lens
            border.color: "black"
            visible: false
            layer.enabled: true
        }
        MultiEffect {
            anchors.fill: parent
            source: band
            blurEnabled: true
            blurMax: 16
            blur: 0.7
            autoPaddingEnabled: false
            maskEnabled: true
            maskThresholdMin: 0.5
            maskSpreadAtMin: 1
            maskSource: shape
        }
    }
    MultiEffect {
        anchors.fill: parent
        visible: glass.live && glass.liquid
        source: beyond
        blurEnabled: true
        blurMax: 16
        blur: 0.45
        saturation: 0.6
        brightness: T.dark ? 0.02 : 0.07
        maskEnabled: true
        maskThresholdMin: 0.5
        maskSpreadAtMin: 1
        maskSource: rim
        autoPaddingEnabled: false
    }

    // The wash, a sheen from above and the shade at the foot.
    Rectangle {
        anchors.fill: parent
        radius: glass.radius
        color: !glass.live ? "transparent"
             : glass.liquid ? T.glass
             // The window's own tint; over an opaque window, the page colour let thin.
             : Chrome.translucent ? T.bgTint : Qt.rgba(T.bg.r, T.bg.g, T.bg.b, 0.72)
    }
    // What keeps the plain panel from melting into the background it is made of.
    Rectangle {
        anchors.fill: parent
        radius: glass.radius
        visible: glass.live && !glass.liquid
        color: T.fill
    }
    Rectangle {
        anchors.fill: parent
        radius: glass.radius
        visible: glass.live && glass.liquid
        gradient: Gradient {
            GradientStop { position: 0; color: Qt.rgba(1, 1, 1, T.dark ? 0.07 : 0.32) }
            GradientStop { position: 0.45; color: Qt.rgba(1, 1, 1, 0) }
            GradientStop { position: 0.7; color: Qt.rgba(T.glassShade.r, T.glassShade.g, T.glassShade.b, 0) }
            GradientStop { position: 1; color: T.glassShade }
        }
    }
    // The edge: a hairline all round, and the light it catches along the top.
    Rectangle {
        anchors.fill: parent
        radius: glass.radius
        color: "transparent"
        border.width: 1
        border.color: Theme.highContrast ? T.borderStrong : glass.liquid ? T.glassEdge : T.border
    }
    Item {
        id: fade
        anchors.fill: parent
        visible: false
        layer.enabled: true
        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0; color: "black" }
                GradientStop { position: 0.5; color: "transparent" }
            }
        }
    }
    Rectangle {
        id: highlight
        anchors.fill: parent
        visible: false
        layer.enabled: true
        radius: glass.radius
        color: "transparent"
        border.width: 1.5
        border.color: T.glassHighlight
    }
    MultiEffect {
        anchors.fill: parent
        visible: glass.live && glass.liquid
        source: highlight
        maskEnabled: true
        maskThresholdMin: 0.5
        maskSpreadAtMin: 1
        maskSource: fade
        autoPaddingEnabled: false
    }
}
