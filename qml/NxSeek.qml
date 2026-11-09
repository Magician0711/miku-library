// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Templates as Tpl

// A slider for time and volume: thin at rest, thicker under the pointer.
Tpl.Slider {
    id: c
    property string name: ""
    implicitWidth: 120
    implicitHeight: 28
    hoverEnabled: true
    focusPolicy: Qt.TabFocus
    Accessible.name: name
    HoverHandler { cursorShape: Qt.PointingHandCursor }
    readonly property bool lively: hovered || pressed || visualFocus

    background: Item {
        x: c.leftPadding
        y: c.topPadding + (c.availableHeight - height) / 2
        width: c.availableWidth
        height: c.lively ? 6 : 4
        Behavior on height { NumberAnimation { duration: T.fast; easing.type: Easing.OutCubic } }
        Rectangle { anchors.fill: parent; radius: height / 2; color: T.fillPressed }
        Rectangle {
            width: c.visualPosition * parent.width
            height: parent.height
            radius: height / 2
            color: T.accentText
        }
    }
    handle: Rectangle {
        x: c.leftPadding + c.visualPosition * (c.availableWidth - width)
        y: c.topPadding + (c.availableHeight - height) / 2
        width: 14; height: 14; radius: 7
        color: T.text
        opacity: c.lively ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: T.fast } }
        FocusRing { target: c; corner: 7 }
    }
}
