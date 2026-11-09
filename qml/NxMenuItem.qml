// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Templates as Tpl

Tpl.MenuItem {
    id: c
    property string iconName: ""
    property color tint: T.text
    implicitWidth: 210
    implicitHeight: 32
    leftPadding: 32
    hoverEnabled: true
    readonly property color ink: !enabled ? T.disabledText : highlighted ? T.accentInk : T.text
    HoverHandler { cursorShape: Qt.PointingHandCursor }
    background: Rectangle {
        radius: 7
        color: c.highlighted && c.enabled ? T.accent : "transparent"
    }
    contentItem: Text {
        text: c.text
        color: c.ink
        font.pixelSize: T.body
        verticalAlignment: Text.AlignVCenter
    }
    indicator: Item {}
    Icon { x: 9; y: (c.height - 16) / 2; size: 16; name: c.iconName; tint: c.highlighted ? T.accentInk : c.enabled ? c.tint : T.disabledText; visible: c.iconName !== "" && !c.checked }
    Icon { x: 9; y: (c.height - 14) / 2; size: 14; name: "check"; tint: c.ink; visible: c.checked }
}
