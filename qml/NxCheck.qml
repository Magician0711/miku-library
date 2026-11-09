// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Templates as Tpl

Tpl.CheckBox {
    id: c
    implicitWidth: 17 + (text !== "" ? label.implicitWidth + 8 : 0)
    implicitHeight: 24
    padding: 0
    hoverEnabled: true
    focusPolicy: Qt.TabFocus
    HoverHandler { cursorShape: Qt.PointingHandCursor }
    indicator: Rectangle {
        y: (c.height - 17) / 2
        width: 17; height: 17; radius: 5
        color: c.checked ? T.accent : T.fill
        border.width: c.checked ? 0 : 1
        border.color: c.hovered ? T.borderStrong : T.border
        FocusRing { target: c; corner: 5 }
        Icon { anchors.centerIn: parent; size: 13; name: "check"; tint: T.accentInk; visible: c.checked }
    }
    contentItem: Text {
        id: label
        leftPadding: 25
        text: c.text
        visible: text !== ""
        color: c.enabled ? T.text : T.disabledText
        font.pixelSize: T.body
        verticalAlignment: Text.AlignVCenter
    }
}
