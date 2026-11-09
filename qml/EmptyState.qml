// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only
import QtQuick

Item {
    id: root
    property string iconName: "search"
    property string title: ""
    property string description: ""
    property bool shown: true
    // The next step from here, when there is an obvious one.
    property string actionText: ""
    property string actionIcon: ""
    property string actionKind: "soft"
    signal action()
    opacity: shown ? 1 : 0
    visible: opacity > 0
    Behavior on opacity { NumberAnimation { duration: T.normal; easing.type: Easing.OutCubic } }
    Column {
        anchors.centerIn: parent
        spacing: 6
        width: Math.min(420, root.width - 48)
        Icon { anchors.horizontalCenter: parent.horizontalCenter; size: 40; name: root.iconName; tint: T.text3 }
        Item { width: 1; height: 8 }
        Text {
            width: parent.width
            text: root.title
            color: T.text
            font.pixelSize: T.title2
            font.weight: Font.DemiBold
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
        }
        Text {
            width: parent.width
            text: root.description
            visible: text !== ""
            color: T.text3
            font.pixelSize: T.body
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
        }
        Item { width: 1; height: 8; visible: root.actionText !== "" }
        NxButton {
            objectName: "emptyAction"
            anchors.horizontalCenter: parent.horizontalCenter
            visible: root.actionText !== ""
            kind: root.actionKind
            iconName: root.actionIcon
            text: root.actionText
            onClicked: root.action()
        }
    }
}
