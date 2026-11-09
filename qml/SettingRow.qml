// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only
import QtQuick

// One setting: title and explanation on the left, its control on the right.
Item {
    id: row
    property string title: ""
    property string hint: ""
    property bool divided: true
    property bool wide: false       // the control takes the remaining width
    default property alias control: holder.data
    Accessible.role: Accessible.Grouping
    Accessible.name: title
    Accessible.description: hint
    width: parent ? parent.width : 0
    height: Math.max(copy.implicitHeight, holder.height) + (divided ? 12 : 0)
    Divider { width: parent.width; visible: row.divided }
    Column {
        id: copy
        y: row.divided ? 12 : 0
        width: row.wide ? parent.width * 0.36 : parent.width - holder.width - 16
        spacing: 2
        Text { width: parent.width; text: row.title; color: T.text; font.pixelSize: T.body; wrapMode: Text.Wrap }
        Text { width: parent.width; text: row.hint; visible: text !== ""; color: T.text3; font.pixelSize: T.callout; wrapMode: Text.Wrap }
    }
    Item {
        id: holder
        anchors.right: parent.right
        y: row.divided ? 12 : 0
        width: row.wide ? parent.width * 0.6 : childrenRect.width
        height: childrenRect.height
    }
}
