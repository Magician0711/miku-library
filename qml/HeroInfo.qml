// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only
import QtQuick

// Title, facts, summary and the anime's actions, used beside or under the poster.
Column {
    id: info
    readonly property var detail: App.detail
    readonly property var hero: detail.hero
    spacing: 8
    property bool open: false
    property int titleSize: T.largeTitle
    property int summaryLines: 4
    Connections {
        target: info.detail
        function onHeroChanged() { if (info.shownId !== info.hero.subjectId) { info.shownId = info.hero.subjectId; info.open = false } }
    }
    property int shownId: 0

    BodyText {
        width: parent.width
        text: info.hero.title || ""
        color: T.text
        font.pixelSize: info.titleSize
        font.weight: Font.Bold
    }
    BodyText {
        width: parent.width
        text: info.hero.subtitle || ""
        visible: text !== ""
        color: T.text2
    }
    Flow {
        width: parent.width
        spacing: 6
        visible: (info.hero.stats || []).length > 0
        Repeater { model: info.hero.stats || []; Chip { required property var modelData; info: modelData } }
    }
    Flow {
        width: parent.width
        spacing: 6
        visible: (info.hero.tags || []).length > 0
        Repeater { model: info.hero.tags || []; Chip { required property var modelData; info: modelData } }
    }
    Item {
        id: box
        width: parent.width
        clip: true
        readonly property real collapsed: Math.min(summary.implicitHeight, lines.lineSpacing * info.summaryLines)
        readonly property bool overflows: summary.implicitHeight > lines.lineSpacing * info.summaryLines + 1
        height: info.open ? summary.implicitHeight : collapsed
        Behavior on height { NumberAnimation { duration: T.normal; easing.type: Easing.OutCubic } }
        FontMetrics { id: lines; font.pixelSize: T.body }
        BodyText { id: summary; width: parent.width; text: (info.hero.summary || "").trim() }
    }
    Item { width: 1; height: 2 }
    Flow {
        width: parent.width
        spacing: 8
        NxButton {
            kind: "primary"
            checked: info.hero.following === true
            text: checked ? "已追番" : "追番"
            iconName: checked ? "check" : "plus"
            onClicked: info.detail.setFollowing(!checked)
        }
        NxButton {
            text: "Bangumi"
            iconName: "external"
            tip: "打开 Bangumi 页面"
            onClicked: info.detail.openPage(info.hero.bangumiUrl)
        }
        NxButton {
            objectName: "summaryMore"
            visible: box.overflows
            text: info.open ? "收起" : "更多"
            iconName: info.open ? "chevron-up" : "chevron-down"
            tip: info.open ? "收起简介" : "展开完整简介"
            onClicked: info.open = !info.open
        }
        NxButton {
            // Only shown once something was downloaded, since updates follow that group.
            id: updates
            visible: info.hero.updatesVisible === true
            text: info.hero.updatesText || ""
            iconName: info.hero.updatesIcon || ""
            tip: info.hero.updatesTip || ""
            onClicked: updatesMenu.popup(updates, 0, updates.height + 4)
            NxMenu {
                id: updatesMenu
                NxMenuItem { text: "不追更"; checkable: true; checked: info.hero.updatesMode === 0; onTriggered: info.detail.setUpdatesMode(0) }
                NxMenuItem { text: "有新集时提醒"; checkable: true; checked: info.hero.updatesMode === 1; onTriggered: info.detail.setUpdatesMode(1) }
                NxMenuItem { text: "有新集时自动下载"; checkable: true; checked: info.hero.updatesMode === 2; onTriggered: info.detail.setUpdatesMode(2) }
            }
        }
    }
}
