// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Effects
import QtQuick.Window

// Artwork as atmosphere: the cover, heavily blurred, spreads across the top of the window,
// then dissolves into the page.
Item {
    id: root
    property string cover: ""
    width: parent ? parent.width : 0
    height: 460
    visible: cover !== ""
    clip: true
    // The window's corners stay put while this scrolls, so the mask moves against it. A
    // see-through window is rounded by the system, which clips everything in it.
    layer.enabled: Shell.corner > 0 && !Chrome.translucent
    layer.effect: MultiEffect {
        maskEnabled: true
        maskSource: corners
        maskThresholdMin: 0.5
        maskSpreadAtMin: 1
        autoPaddingEnabled: false
    }
    Item {
        id: corners
        anchors.fill: parent
        visible: false
        layer.enabled: true
        clip: true
        Rectangle {
            y: -root.y
            width: root.width
            height: root.height - y
            topLeftRadius: Shell.corner
            topRightRadius: Shell.corner
            color: "black"
        }
    }
    // The blur reaches past what it is given and fades to nothing there, so the artwork
    // runs a blur's width beyond the sides and top and the clip trims it back.
    readonly property int bleed: 64
    Image {
        id: art
        x: -root.bleed; y: -root.bleed
        width: root.width + 2 * root.bleed
        height: root.height + root.bleed
        visible: false
        // Blurred anyway: a small decode is enough.
        source: root.cover === "" ? "" : "image://cover/0/" + encodeURIComponent(root.cover)
        sourceSize: Qt.size(160, 226)
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
    }
    // Over a see-through window there is no page colour to dissolve into: the artwork
    // itself fades out towards the bottom instead.
    readonly property bool seeThrough: Chrome.translucent
    Item {
        id: fadeOut
        anchors.fill: parent
        visible: false
        layer.enabled: true
        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0; color: "black" }
                GradientStop { position: 0.35; color: "black" }
                GradientStop { position: 1; color: "transparent" }
            }
        }
    }
    // Blurred first, faded after: a blur pads its output, and a mask on the same effect
    // would stretch over that padding and never reach nothing at the bottom.
    Item {
        anchors.fill: parent
        opacity: art.status === Image.Ready ? (T.dark ? 0.6 : 0.55) : 0
        Behavior on opacity { NumberAnimation { duration: T.slow; easing.type: Easing.OutCubic } }
        layer.enabled: root.seeThrough
        layer.effect: MultiEffect {
            maskEnabled: true
            maskThresholdMin: 0.5
            maskSpreadAtMin: 1
            maskSource: fadeOut
            autoPaddingEnabled: false
        }
        MultiEffect {
            x: art.x; y: art.y
            width: art.width; height: art.height
            source: art
            blurEnabled: true
            blurMax: 64
            blur: 1
            saturation: 0.25
        }
    }
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0; color: Qt.rgba(T.bg.r, T.bg.g, T.bg.b, 0.25) }
            GradientStop { position: 0.55; color: Qt.rgba(T.bg.r, T.bg.g, T.bg.b, root.seeThrough ? 0.4 : 0.72) }
            GradientStop { position: 1; color: root.seeThrough ? Qt.rgba(T.bg.r, T.bg.g, T.bg.b, 0) : T.bg }
        }
    }
}
