// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Templates as Tpl

Tpl.Menu {
    id: c
    padding: 5
    implicitWidth: 220
    implicitHeight: contentItem.implicitHeight + 10
    enter: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: T.fast; easing.type: Easing.OutCubic }
            NumberAnimation { property: "scale"; from: 0.97; to: 1; duration: T.fast; easing.type: Easing.OutCubic }
        }
    }
    exit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: T.fast } }
    delegate: NxMenuItem {}
    background: Rectangle {
        radius: 12
        color: T.surface
        border.color: T.border
        Shadow { blur: 18; offset: 6 }
    }
    contentItem: ListView {
        implicitHeight: contentHeight
        model: c.contentModel
        interactive: false
        clip: true
        currentIndex: c.currentIndex
    }
}
