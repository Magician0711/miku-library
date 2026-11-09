// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only
import QtQuick

// Page title and supporting text, with the page's commands on the right.
Item {
    id: root
    property string title: ""
    property string meta: ""
    property string metaTip: ""
    default property alias commands: actions.data
    implicitHeight: Math.max(copy.implicitHeight, actions.implicitHeight)
    Column {
        id: copy
        spacing: 5
        width: parent.width - actions.width - 16
        Text {
            text: root.title
            color: T.text
            font.pointSize: 23
            font.weight: Font.DemiBold
        }
        Text {
            width: parent.width
            text: root.meta
            color: T.text3
            font.pointSize: 10
            elide: Text.ElideRight
            height: Math.max(18, implicitHeight)
            Tip { text: root.metaTip }
        }
    }
    Row {
        id: actions
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        spacing: 8
    }
}
