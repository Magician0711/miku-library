// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Templates as Tpl

// An overlay indicator: it appears while scrolling or under the pointer, and takes no
// room from the content.
Tpl.ScrollBar {
    id: c
    // For a list that fills the window: stays clear of the window controls at the top
    // and of the rounded corners.
    property bool windowEdge: false
    implicitWidth: windowEdge ? 12 : 10
    padding: 2
    rightPadding: windowEdge ? 4 : 2
    topPadding: windowEdge ? 52 : 6
    bottomPadding: windowEdge ? Shell.corner + 6 : 6
    minimumSize: 0.06
    hoverEnabled: true
    visible: size < 1
    contentItem: Rectangle {
        implicitWidth: c.hovered || c.pressed ? 7 : 5
        radius: width / 2
        color: T.text3
        opacity: c.active || c.hovered ? (c.hovered ? 0.7 : 0.45) : 0
        Behavior on opacity { NumberAnimation { duration: T.normal } }
    }
}
