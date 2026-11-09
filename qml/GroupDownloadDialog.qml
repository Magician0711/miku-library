// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only
import QtQuick

// What 一键下载 asks before it starts: subtitle, resolution and an episode window. Every
// change re-runs the real download planner, so the estimate shown is exactly what
// 开始下载 will queue.
NxDialog {
    id: dialog
    objectName: "groupDownloadDialog"
    readonly property var options: App.detail.groupDialog
    readonly property bool wanted: options.group !== undefined
    property var shown: ({})
    title: "选择下载方式"
    dialogWidth: 500

    onWantedChanged: {
        if (wanted) {
            shown = options
            language.value = options.language
            type.value = options.subtitleType
            resolution.value = options.resolution
            first.value = options.low
            last.value = options.high
            refresh()
            open()
        } else {
            close()
        }
    }
    onCloseClicked: App.detail.dismissGroup()

    property string summary: ""
    property bool ready: false
    function choice(name) {
        return name.currentValue === undefined ? "" : name.currentValue
    }
    function refresh() {
        const result = App.detail.previewGroup(choice(language), choice(type), choice(resolution), first.value, last.value)
        summary = result.summary
        ready = result.ready
    }
    component Caption: Text {
        width: 52
        height: 32
        verticalAlignment: Text.AlignVCenter
        color: T.text2
        font.pixelSize: T.body
    }

    Text {
        width: parent.width
        text: dialog.shown.group || ""
        color: T.text3
        font.pixelSize: T.body
        wrapMode: Text.Wrap
    }
    Row {
        width: parent.width
        spacing: 10
        Caption { text: "字幕" }
        NxSelect {
            id: language
            objectName: "groupLanguage"
            width: (parent.width - 72) / 2
            model: dialog.shown.languages || []
            enabled: count > 1
            onActivated: Qt.callLater(dialog.refresh)
        }
        NxSelect {
            id: type
            objectName: "groupSubtitleType"
            width: (parent.width - 72) / 2
            model: dialog.shown.types || []
            enabled: count > 1
            onActivated: Qt.callLater(dialog.refresh)
        }
    }
    Row {
        width: parent.width
        spacing: 10
        Caption { text: "清晰度" }
        NxSelect {
            id: resolution
            objectName: "groupResolution"
            width: parent.width - 62
            model: dialog.shown.resolutions || []
            enabled: count > 1
            onActivated: Qt.callLater(dialog.refresh)
        }
    }
    Row {
        width: parent.width
        spacing: 10
        Caption { text: "集数" }
        NxSpin {
            id: first
            objectName: "groupFirst"
            width: (parent.width - 62 - 28) / 2
            from: 1; to: 1899
            prefix: "第 "; suffix: " 话"
            enabled: dialog.shown.hasEpisodes === true
            onEdited: value => { if (last.value < value) last.value = value; dialog.refresh() }
        }
        Text {
            width: 8
            height: 32
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignHCenter
            text: "至"
            color: T.text3
            font.pixelSize: T.callout
        }
        NxSpin {
            id: last
            objectName: "groupLast"
            width: (parent.width - 62 - 28) / 2
            from: 1; to: 1899
            prefix: "第 "; suffix: " 话"
            enabled: dialog.shown.hasEpisodes === true
            onEdited: value => { if (first.value > value) first.value = value; dialog.refresh() }
        }
    }
    Row {
        leftPadding: 62
        spacing: 6
        Repeater {
            model: dialog.shown.quick || []
            NxButton {
                required property var modelData
                implicitHeight: 28
                text: modelData.text
                enabled: dialog.shown.hasEpisodes === true
                onClicked: { first.value = modelData.from; last.value = modelData.to; dialog.refresh() }
            }
        }
    }
    Text {
        width: parent.width
        visible: dialog.shown.hasEpisodes === false
        text: "这些资源都是集数未知的合集，将按合集内容下载。"
        color: T.text3
        font.pixelSize: T.callout
        wrapMode: Text.Wrap
    }
    Text {
        // Room for a two-line reason, so the dialog keeps its size as options change.
        objectName: "groupSummary"
        width: parent.width
        height: Math.max(implicitHeight, 36)
        text: dialog.summary
        color: T.text2
        font.pixelSize: T.callout
        wrapMode: Text.Wrap
    }
    Item {
        width: parent.width
        height: 32
        Row {
            anchors.right: parent.right
            spacing: 8
            NxButton { text: "取消"; onClicked: App.detail.dismissGroup() }
            NxButton {
                objectName: "groupStart"
                kind: "primary"
                text: "开始下载"
                enabled: dialog.ready
                onClicked: App.detail.confirmGroup(dialog.choice(language), dialog.choice(type), dialog.choice(resolution), first.value, last.value)
            }
        }
    }
}
