// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only
import QtQuick

// A titled group of settings. Its section places it, and places it again when it grows.
Group {
    id: card
    property string title: ""
    default property alias rows: column.data
    height: column.implicitHeight + 30
    onHeightChanged: if (parent && parent.arrange) parent.arrange()
    onVisibleChanged: if (parent && parent.arrange) parent.arrange()
    Column {
        id: column
        x: 18; y: 14
        width: card.width - 36
        spacing: 11
        Text { text: card.title; color: T.text; font.pixelSize: T.title3; font.weight: Font.DemiBold }
    }
}
