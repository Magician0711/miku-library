// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Effects
import QtQuick.Window

// Artwork as atmosphere: the cover, heavily blurred, spreads across the top of the window
// and behind the sidebar, then dissolves into the page.
Item {
    id: root
    property string cover: ""
    width: parent ? parent.width : 0
    height: 460
    visible: cover !== ""
    clip: true
    // The window's corners stay put while this scrolls, so the mask moves against it.
    layer.enabled: Shell.corner > 0
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
    Image {
        id: art
        anchors.fill: parent
        visible: false
        // Blurred anyway: a small decode is enough.
        source: root.cover === "" ? "" : "image://cover/0/" + encodeURIComponent(root.cover)
        sourceSize: Qt.size(160, 226)
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
    }
    MultiEffect {
        anchors.fill: parent
        source: art
        blurEnabled: true
        blurMax: 64
        blur: 1
        saturation: 0.25
        opacity: art.status === Image.Ready ? (T.dark ? 0.6 : 0.55) : 0
        Behavior on opacity { NumberAnimation { duration: T.slow; easing.type: Easing.OutCubic } }
    }
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0; color: Qt.rgba(T.bg.r, T.bg.g, T.bg.b, 0.25) }
            GradientStop { position: 0.55; color: Qt.rgba(T.bg.r, T.bg.g, T.bg.b, 0.72) }
            GradientStop { position: 1; color: T.bg }
        }
    }
}
