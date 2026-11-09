// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Templates as Tpl

Tpl.Button {
    id: c
    property string iconName: ""
    property string tip: ""
    property color tint: T.text
    property int side: 32
    implicitWidth: side; implicitHeight: side
    padding: 0
    hoverEnabled: true
    focusPolicy: Qt.TabFocus
    Accessible.name: tip
    HoverHandler { cursorShape: Qt.PointingHandCursor }
    Tip { text: c.tip }
    background: Rectangle {
        radius: height / 2
        color: c.down ? T.fillPressed : c.hovered && c.enabled ? T.fill : "transparent"
        Behavior on color { ColorAnimation { duration: T.fast } }
        FocusRing { target: c; corner: parent.radius }
    }
    contentItem: Item {
        Icon { anchors.centerIn: parent; size: 17; name: c.iconName; tint: c.enabled ? c.tint : T.disabledText }
    }
}
