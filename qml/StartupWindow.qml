// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Templates as Tpl
import QtQuick.Window

// The welcome guide, or the question for a new download folder, before the app opens.
Window {
    id: window
    readonly property bool guide: Startup.mode === "guide"
    width: 640
    height: guide ? Math.min(screen.height * 0.9 / Screen.devicePixelRatio, stack.implicitHeight + 124) : 300
    title: guide ? "欢迎使用 Miku Library" : "需要更换下载目录"
    color: "transparent"
    flags: Qt.Window | Qt.FramelessWindowHint | Qt.WindowSystemMenuHint | Qt.WindowCloseButtonHint
    property int step: 0
    visible: false

    Rectangle { parent: window.contentItem; anchors.fill: parent; radius: Shell.corner; color: T.bg }
    Rectangle {
        parent: window.contentItem
        anchors.fill: parent
        z: 2500000
        radius: Shell.corner
        color: "transparent"
        border.width: 1
        border.color: T.glassEdge
    }
    Component.onCompleted: {
        Chrome.setDark(Theme.dark)
        Chrome.attach(window)
        visible = true
    }
    Connections {
        target: Theme
        function onDarkChanged() { Chrome.setDark(Theme.dark) }
    }
    onClosing: close => { close.accepted = false; Startup.finish(false) }

    component GuideRow: Item {
        id: row
        property string glyph
        property bool warning: false
        property string title
        property string body
        default property alias trailing: trailingHolder.data
        width: parent ? parent.width : 0
        height: Math.max(26, copy.implicitHeight)
        Icon { y: 1; size: 22; name: row.glyph; tint: row.warning ? T.warn : T.accentText }
        Column {
            id: copy
            x: 36
            width: parent.width - 36 - trailingHolder.width - (trailingHolder.width > 0 ? 12 : 0)
            spacing: 1
            Text { text: row.title; color: T.text; font.pixelSize: T.body; font.weight: Font.DemiBold }
            BodyText { width: parent.width; text: row.body; color: T.text2; font.pixelSize: T.callout }
        }
        Item {
            id: trailingHolder
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            width: childrenRect.width
            height: childrenRect.height
        }
    }
    component GuideSection: Group {
        id: section
        default property alias rows: card.data
        width: parent ? parent.width : 0
        height: card.implicitHeight + 24
        Column {
            id: card
            x: 16; y: 12
            width: parent.width - 32
            spacing: 10
        }
    }

    Item {
        anchors.fill: parent
        Item {
            id: bar
            width: parent.width; height: 52
            DragHandler { target: null; onActiveChanged: if (active) window.startSystemMove() }
            Text {
                x: 24
                anchors.verticalCenter: parent.verticalCenter
                text: window.title
                color: T.text
                font.pixelSize: T.title1
                font.weight: Font.Bold
            }
            WindowButtons { anchors.right: parent.right; window: window; closeOnly: true }
        }

        Item {
            id: stack
            x: 24; y: bar.height + 4
            width: parent.width - 48
            height: parent.height - y - 68
            implicitHeight: Math.max(start.implicitHeight, 420)
            // Page 1: quick start, two things to know, then where to save and how it looks.
            Column {
                id: start
                width: parent.width
                spacing: 10
                visible: window.guide && opacity > 0
                opacity: window.step === 0 ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: T.fast } }
                BodyText { width: parent.width; text: "聚合多个资源站，按字幕组整理番剧资源，并直接管理下载与本地视频。" }
                GuideSection {
                    GuideRow { glyph: "calendar"; title: "找番"; body: "在「本季放送」按星期浏览新番，或按 Ctrl+K 用中文、日文或英文番名搜索。" }
                    Divider { width: parent.width }
                    GuideRow { glyph: "filter"; title: "选字幕组"; body: "番剧详情优先显示人气高、资源多的字幕组，可按季度、清晰度、字幕和关键词筛选。" }
                    Divider { width: parent.width }
                    GuideRow { glyph: "download"; title: "下载与观看"; body: "点「下载」获取单集，或用「一键下载」补齐缺集，并在弹窗里选字幕、清晰度和集数范围；完成后在「我的追番」点「播放」。" }
                }
                GuideSection {
                    GuideRow { glyph: "play"; title: "关于播放"; body: Startup.playerNote }
                    Divider { width: parent.width }
                    GuideRow { glyph: "info"; warning: true; title: "免费开源"; body: Startup.freeNote }
                }
                GuideSection {
                    GuideRow {
                        glyph: "folder"
                        title: "下载位置"
                        body: Startup.downloadDir.replace(/\//g, "\\")
                        NxButton { text: "更改…"; onClicked: Startup.chooseDirectory() }
                    }
                    Divider { width: parent.width }
                    GuideRow {
                        glyph: "sun"
                        title: "外观"
                        body: "之后可在「偏好设置」修改。"
                        Segmented {
                            compact: true
                            items: [{ text: "跟随系统", value: "system" }, { text: "浅色", value: "light" }, { text: "深色", value: "dark" }]
                            value: Theme.mode
                            onActivated: value => Theme.setMode(value)
                        }
                    }
                }
            }
            // Page 2: the declaration, which must be accepted.
            Column {
                id: terms
                width: parent.width
                spacing: 12
                visible: window.guide && opacity > 0
                opacity: window.step === 1 ? 1 : 0
                Behavior on opacity { NumberAnimation { duration: T.fast } }
                Group {
                    width: parent.width
                    height: stack.height - 40
                    Flickable {
                        id: termsView
                        anchors.fill: parent
                        anchors.margins: 16
                        contentHeight: list.implicitHeight
                        clip: true
                        interactive: false
                        boundsBehavior: Flickable.StopAtBounds
                        Tpl.ScrollBar.vertical: NxScrollBar {}
                        SmoothWheel { view: termsView }
                        Column {
                            id: list
                            width: termsView.width - 10
                            spacing: 10
                            Repeater {
                                model: Startup.disclaimer
                                Row {
                                    id: item
                                    required property string modelData
                                    required property int index
                                    width: list.width
                                    spacing: 10
                                    Text { width: 18; text: item.index + 1; color: T.accentText; font.pixelSize: T.body; font.weight: Font.Bold }
                                    BodyText { width: parent.width - 28; text: item.modelData; color: T.text }
                                }
                            }
                        }
                    }
                }
                NxCheck { id: agree; text: "我已阅读并同意以上声明" }
            }
            // A saved folder that is gone.
            Column {
                width: parent.width
                spacing: 12
                visible: !window.guide
                BodyText { width: parent.width; text: Startup.reason }
                Group {
                    width: parent.width
                    height: location.implicitHeight + 24
                    BodyText { id: location; x: 14; y: 12; width: parent.width - 28; text: Startup.downloadDir.replace(/\//g, "\\"); color: T.text }
                }
            }
        }

        Item {
            x: 24
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 18
            width: parent.width - 48
            height: 32
            Text {
                anchors.verticalCenter: parent.verticalCenter
                visible: window.guide
                text: (window.step + 1) + " / 2"
                color: T.text3
                font.pixelSize: T.callout
            }
            Row {
                anchors.right: parent.right
                spacing: 8
                NxButton { visible: window.guide && window.step === 1; text: "上一步"; onClicked: window.step = 0 }
                NxButton { visible: window.guide && window.step === 1; kind: "ghost"; text: "不同意并退出"; onClicked: Startup.finish(false) }
                NxButton {
                    visible: window.guide
                    kind: "primary"
                    text: window.step === 1 ? "同意并开始使用" : "下一步"
                    enabled: window.step === 0 || agree.checked
                    onClicked: window.step === 1 ? Startup.finish(true) : window.step = 1
                }
                NxButton { visible: !window.guide; text: "退出软件"; onClicked: Startup.finish(false) }
                NxButton { visible: !window.guide; kind: "primary"; text: "选择新目录"; onClicked: Startup.chooseDirectory() }
            }
        }
    }
    TipLayer { parent: window.contentItem }
    ThemeFade { parent: window.contentItem }
}
