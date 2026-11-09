// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Templates as Tpl

// Picks the downloader for new tasks, next to the control that opened it.
Tpl.Popup {
    id: popup
    width: 320
    implicitHeight: implicitContentHeight + 12
    padding: 6
    modal: false
    focus: true
    closePolicy: Tpl.Popup.CloseOnEscape | Tpl.Popup.CloseOnPressOutside
    property var options: []
    function showAt(anchor) {
        options = App.downloaderOptions()
        parent = Tpl.Overlay.overlay
        const p = anchor.mapToItem(parent, 0, 0)
        const h = options.length * 52 + 80
        x = Math.max(6, Math.min(parent.width - width - 6, p.x - 10))
        y = p.y + anchor.height + 6 + h < parent.height ? p.y + anchor.height + 6 : Math.max(6, p.y - h - 6)
        open()
    }
    enter: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: T.fast; easing.type: Easing.OutCubic }
            NumberAnimation { property: "scale"; from: 0.97; to: 1; duration: T.fast; easing.type: Easing.OutCubic }
        }
    }
    exit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: T.fast } }
    background: Rectangle {
        radius: 14
        color: T.surface
        border.color: T.border
        Shadow { blur: 20; offset: 6 }
    }
    contentItem: Column {
        spacing: 2
        Text { leftPadding: 10; topPadding: 6; bottomPadding: 4; text: "选择下载器"; color: T.text3; font.pixelSize: T.callout; font.weight: Font.DemiBold }
        Repeater {
            model: popup.options
            Rectangle {
                id: row
                required property var modelData
                width: popup.availableWidth
                height: 50
                radius: 9
                color: rowHover.hovered ? T.fill : "transparent"
                Accessible.role: Accessible.Button
                Accessible.name: modelData.name
                Accessible.onPressAction: { popup.close(); App.chooseDownloader(modelData.type) }
                Icon { x: 10; anchors.verticalCenter: parent.verticalCenter; size: 14; name: "check"; tint: T.accentText; visible: row.modelData.current }
                Column {
                    x: 32
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 1
                    Text { text: row.modelData.name; color: T.text; font.pixelSize: T.body; font.weight: Font.Medium }
                    Text { text: row.modelData.description; color: T.text3; font.pixelSize: T.callout }
                }
                Text {
                    anchors.right: parent.right; anchors.rightMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    visible: !row.modelData.current && !row.modelData.configured
                    text: "未配置"
                    color: T.text3
                    font.pixelSize: T.callout
                }
                HoverHandler { id: rowHover; cursorShape: Qt.PointingHandCursor }
                TapHandler { onTapped: { popup.close(); App.chooseDownloader(row.modelData.type) } }
            }
        }
        Item { width: 1; height: 3 }
        Divider { width: popup.availableWidth }
        Item { width: 1; height: 3 }
        Rectangle {
            width: popup.availableWidth
            height: 34
            radius: 9
            color: configureHover.hovered ? T.fill : "transparent"
            Text { x: 32; anchors.verticalCenter: parent.verticalCenter; text: "配置下载器…"; color: T.text; font.pixelSize: T.body }
            HoverHandler { id: configureHover; cursorShape: Qt.PointingHandCursor }
            TapHandler { onTapped: { popup.close(); App.chooseDownloader("configure") } }
        }
    }
}
