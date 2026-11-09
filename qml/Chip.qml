// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only
import QtQuick

Rectangle {
    id: root
    // {text, bg, fg, tip}; bg and fg name colours in T.
    property var info: ({})
    radius: height / 2
    width: label.implicitWidth + 16
    height: label.implicitHeight + 5
    color: T[info.bg || "surface2"] || T.surface2
    Text {
        id: label
        anchors.centerIn: parent
        text: root.info.text || ""
        color: T[root.info.fg || "text2"] || T.text2
        font.pixelSize: T.footnote
        font.weight: Font.Medium
    }
    Tip { text: root.info.tip || "" }
}
