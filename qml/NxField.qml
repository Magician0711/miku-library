// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Templates as Tpl

Tpl.TextField {
    id: c
    property string iconName: ""
    property bool clearable: false
    property bool plain: false      // no frame of its own, for the search bar
    implicitWidth: 200
    implicitHeight: 32
    leftPadding: plain ? 4 : iconName !== "" ? 32 : 12
    rightPadding: clearable && text !== "" ? 32 : plain ? 4 : 12
    topPadding: 0; bottomPadding: 0
    color: enabled ? T.text : T.disabledText
    selectionColor: T.accentSoft2
    selectedTextColor: T.text
    placeholderTextColor: T.text3
    // Templates take the platform font, not the application one, and Chinese text would
    // then fall back to several fonts within a line.
    font: Qt.application.font
    verticalAlignment: TextInput.AlignVCenter
    selectByMouse: true
    hoverEnabled: true
    background: Rectangle {
        visible: !c.plain
        radius: height / 2
        color: T.fill
        border.width: c.activeFocus ? 2 : 0
        border.color: T.accent
    }
    Text {
        x: c.leftPadding
        width: c.width - c.leftPadding - c.rightPadding
        height: c.height
        verticalAlignment: Text.AlignVCenter
        text: c.placeholderText
        color: c.placeholderTextColor
        font: c.font
        elide: Text.ElideRight
        visible: c.text === "" && c.preeditText === ""
    }
    Icon { x: 11; y: (c.height - 16) / 2; size: 16; name: c.iconName; tint: T.text3; visible: c.iconName !== "" }
    IconButton {
        x: c.width - width - 4; y: (c.height - height) / 2
        side: 24
        iconName: "close"
        tint: T.text3
        tip: "清除"
        visible: c.clearable && c.text !== ""
        focusPolicy: Qt.NoFocus
        onClicked: c.clear()
    }
}
