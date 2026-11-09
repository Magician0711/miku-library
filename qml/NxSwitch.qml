// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Templates as Tpl

Tpl.Switch {
    id: c
    implicitWidth: 40 + (text !== "" ? label.implicitWidth + 8 : 0)
    implicitHeight: 24
    padding: 0
    hoverEnabled: true
    focusPolicy: Qt.TabFocus
    HoverHandler { cursorShape: Qt.PointingHandCursor }
    indicator: Rectangle {
        width: 40; height: 24; radius: 12
        color: c.checked ? T.accent : T.surface3
        opacity: c.enabled ? 1 : 0.5
        Behavior on color { ColorAnimation { duration: T.fast } }
        FocusRing { target: c; corner: 12 }
        Rectangle {
            width: 20; height: 20; radius: 10
            y: 2
            x: c.checked ? 18 : 2
            color: "#ffffff"
            Behavior on x { NumberAnimation { duration: T.normal; easing.type: Easing.OutBack; easing.overshoot: 1.1 } }
            Shadow { blur: 3; offset: 1 }
        }
    }
    contentItem: Text {
        id: label
        leftPadding: 48
        text: c.text
        visible: text !== ""
        color: c.enabled ? T.text : T.disabledText
        font.pixelSize: T.body
        verticalAlignment: Text.AlignVCenter
    }
}
