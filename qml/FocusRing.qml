// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only
import QtQuick

// Keyboard focus only: a ring just outside the control, concentric with its corners.
Rectangle {
    required property Item target
    property real corner: 8
    anchors.fill: parent
    anchors.margins: -3
    radius: corner + 3
    color: "transparent"
    border.width: 2
    border.color: T.accentText   // the brand colour itself is only 1.9:1 on the light page
    visible: target.visualFocus === true
}
