// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Window

// Cross-dissolves a theme switch. A still of the window is taken first, and the palette
// flips underneath it once that still is on screen; it then fades out on the GPU. The
// still is taken of the window with this layer included, so a switch during a fade
// carries on from the colours showing instead of jumping.
ShaderEffectSource {
    id: fade
    anchors.fill: parent
    z: 3000000
    sourceItem: parent
    live: false
    recursive: true
    visible: false
    opacity: 0

    property bool waiting: false
    Component.onCompleted: Theme.fading = true
    Component.onDestruction: Theme.fading = false

    function start() {
        running.stop()
        opacity = Math.max(opacity, 0.004)
        visible = true
        waiting = true
        scheduleUpdate()
    }
    Connections {
        target: Theme
        function onSwitchRequested() { fade.start() }
    }
    Connections {
        target: fade.Window.window
        enabled: fade.waiting
        function onFrameSwapped() {
            fade.waiting = false
            fade.opacity = 1
            Theme.commit()
            Chrome.beginTrace("theme")
            running.restart()
        }
    }
    NumberAnimation {
        id: running
        target: fade
        property: "opacity"
        to: 0
        duration: T.normal
        easing.type: Easing.OutCubic
        onFinished: { fade.visible = false; Chrome.endTrace() }
    }
}
