// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Templates as Tpl

// A sheet: one short task over a dimmed window, left with its own buttons or Esc.
Tpl.Popup {
    id: dialog
    property string title: ""
    property int dialogWidth: 480
    default property alias content: body.data
    signal closeClicked()

    parent: Tpl.Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(dialogWidth, parent ? parent.width - 48 : dialogWidth)
    implicitHeight: implicitContentHeight
    padding: 0
    modal: true
    closePolicy: Tpl.Popup.NoAutoClose
    focus: true
    Shortcut { sequence: "Esc"; enabled: dialog.visible; onActivated: dialog.closeClicked() }
    Tpl.Overlay.modal: Rectangle { radius: Shell.corner; color: Qt.rgba(0, 0, 0, T.dark ? 0.45 : 0.25) }
    enter: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: T.fast; easing.type: Easing.OutCubic }
            NumberAnimation { property: "scale"; from: 0.96; to: 1; duration: T.normal; easing.type: Easing.OutCubic }
        }
    }
    exit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: T.fast } }
    background: Rectangle {
        radius: 20
        color: T.surface
        border.color: T.border
        Shadow { blur: 30; offset: 10 }
    }
    contentItem: Column {
        leftPadding: 24
        rightPadding: 24
        topPadding: 22
        bottomPadding: 22
        spacing: 12
        Text {
            width: dialog.width - 48
            text: dialog.title
            color: T.text
            font.pixelSize: T.title2
            font.weight: Font.DemiBold
            wrapMode: Text.Wrap
        }
        Column {
            id: body
            width: dialog.width - 48
            spacing: 14
        }
    }
}
