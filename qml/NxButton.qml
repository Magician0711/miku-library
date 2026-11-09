// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Templates as Tpl

Tpl.Button {
    id: c
    // default, primary, soft, ghost, danger
    property string kind: "default"
    property string iconName: ""
    property string tip: ""
    property bool busy: false
    property real iconSize: 16
    property int fontWeight: kind === "primary" || kind === "soft" ? Font.DemiBold : Font.Medium

    readonly property color inkColor: !enabled ? T.disabledText
        : kind === "primary" ? (checked ? T.accentText : T.accentInk)
        : kind === "soft" ? T.accentText
        : kind === "danger" ? T.err : T.text

    implicitWidth: Math.max(implicitContentWidth + leftPadding + rightPadding, 32)
    implicitHeight: 32
    leftPadding: 14; rightPadding: 14
    topPadding: 0; bottomPadding: 0
    hoverEnabled: true
    focusPolicy: Qt.TabFocus
    Accessible.name: text !== "" ? text : tip

    HoverHandler { cursorShape: Qt.PointingHandCursor }
    Tip { text: c.tip }

    background: Rectangle {
        radius: height / 2
        color: {
            if (c.kind === "primary") {
                if (!c.enabled) return T.fill
                if (c.checked) return T.accentSoft
                return c.down ? T.accentPressed : c.hovered ? T.accentHover : T.accent
            }
            if (c.kind === "soft")
                return !c.enabled ? T.fill : c.hovered ? T.accentSoft2 : T.accentSoft
            if (c.kind === "ghost")
                return c.down ? T.fillPressed : c.hovered && c.enabled ? T.fill : "transparent"
            if (c.kind === "danger" && c.hovered && c.enabled)
                return T.errSoft
            return c.down ? T.fillPressed : c.hovered && c.enabled ? T.fillHover : T.fill
        }
        Behavior on color { ColorAnimation { duration: T.fast } }
        FocusRing { target: c; corner: parent.radius }
    }
    contentItem: Item {
        implicitWidth: row.implicitWidth
        implicitHeight: row.implicitHeight
        Row {
            id: row
            anchors.centerIn: parent
            spacing: 6
            Item {
                width: c.iconName !== "" || c.busy ? c.iconSize : 0
                height: c.iconSize
                anchors.verticalCenter: parent.verticalCenter
                visible: width > 0
                Icon { anchors.fill: parent; size: c.iconSize; name: c.iconName; tint: c.inkColor; visible: !c.busy }
                Spinner { anchors.fill: parent; size: c.iconSize; visible: c.busy }
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: c.text
                visible: text !== ""
                color: c.inkColor
                font.pixelSize: T.body
                font.weight: c.fontWeight
            }
        }
    }
}
