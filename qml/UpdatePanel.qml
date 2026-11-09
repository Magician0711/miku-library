// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Templates as Tpl

// New episodes from followed groups, bottom-right over any page. Rows stay until they
// are handled or dismissed.
Rectangle {
    id: panel
    width: 380
    height: column.implicitHeight + 26
    radius: 16
    color: T.surface
    border.color: T.border
    opacity: App.noticesOpen ? 1 : 0
    visible: opacity > 0
    z: 10
    Behavior on opacity { NumberAnimation { duration: T.normal; easing.type: Easing.OutCubic } }
    Shadow { blur: 24; offset: 8 }
    TapHandler {}
    Column {
        id: column
        x: 16; y: 12
        width: parent.width - 24
        spacing: 8
        Item {
            width: parent.width; height: 34
            Column {
                anchors.verticalCenter: parent.verticalCenter
                Text { text: "追番有新集"; color: T.text; font.pixelSize: T.title3; font.weight: Font.DemiBold }
                Text { text: App.noticeSummary; color: T.text3; font.pixelSize: T.callout }
            }
            IconButton {
                anchors.right: parent.right
                side: 28
                iconName: "close"
                tint: T.text2
                tip: "暂时关闭（下次启动会再次检查）"
                onClicked: App.closeNotices()
            }
        }
        ListView {
            id: rows
            width: parent.width
            height: Math.min(App.notices.length, 4) * 56
            model: App.notices
            clip: true
            interactive: false
            boundsBehavior: Flickable.StopAtBounds
            Tpl.ScrollBar.vertical: NxScrollBar {}
            SmoothWheel { view: rows }
            delegate: Item {
                id: row
                required property var modelData
                required property int index
                width: ListView.view.width - 8
                height: 56
                Divider { width: parent.width; visible: row.index > 0 }
                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - actions.width - 8
                    spacing: 1
                    Text { width: parent.width; text: row.modelData.title; color: T.text; font.pixelSize: T.body; font.weight: Font.DemiBold; elide: Text.ElideRight; Tip { text: row.modelData.title } }
                    Text { width: parent.width; text: row.modelData.detail; color: T.text3; font.pixelSize: T.callout; elide: Text.ElideRight }
                }
                Row {
                    id: actions
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 6
                    NxButton { kind: "ghost"; text: "忽略"; tip: "不再提示这些集数，之后有更新的集时仍会提醒"; onClicked: App.ignoreNotice(row.modelData.subjectId) }
                    NxButton { kind: "soft"; text: "下载"; onClicked: App.downloadNotice(row.modelData.subjectId) }
                }
            }
        }
        Item {
            width: parent.width - 8
            height: all.height
            visible: App.notices.length > 1
            NxButton { id: all; anchors.right: parent.right; kind: "primary"; text: "全部下载"; iconName: "download"; onClicked: App.downloadAllNotices() }
        }
    }
}
