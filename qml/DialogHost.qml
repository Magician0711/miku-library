// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Templates as Tpl

// Draws whatever DialogHub wants answered.
NxDialog {
    id: host
    readonly property var spec: App.dialogs.current
    readonly property string kind: spec.kind || ""
    property var shownSpec: ({})
    readonly property string shownKind: shownSpec.kind || ""
    title: shownSpec.title || ""
    dialogWidth: shownKind === "video" ? 600 : 460

    onSpecChanged: {
        // Read from spec itself: the kind binding may not have caught up yet.
        if ((spec.kind || "") !== "") {
            shownSpec = spec
            remember.checked = false
            videoList.currentIndex = 0
            open()
        } else {
            close()
        }
    }
    function dismiss() {
        App.dialogs.answer(shownKind === "ask" ? "reject" : shownKind === "video" ? "" : shownKind === "message" ? "" : "cancel", false)
    }
    onCloseClicked: dismiss()

    // message
    Row {
        width: parent.width
        spacing: 12
        visible: host.shownKind === "message"
        Icon {
            readonly property string notice: host.shownSpec.notice || "info"
            size: 26
            name: notice === "error" ? "error" : notice === "warning" ? "warning" : "info"
            tint: notice === "error" ? T.err : notice === "warning" ? T.warn : T.accentText
        }
        BodyText { width: parent.width - 38; text: host.shownSpec.text || "" }
    }
    BodyText {
        width: parent.width
        visible: host.shownKind === "ask" || host.shownKind === "removal" || host.shownKind === "close"
        text: host.shownSpec.text || ""
    }
    NxCheck {
        id: remember
        visible: host.shownKind === "removal" || host.shownKind === "close"
        text: host.shownKind === "removal" ? "记住我的选择（可在「偏好设置 → 下载」中修改）"
                                           : "记住我的选择（可在「偏好设置 → 通用」中修改）"
    }
    Rectangle {
        visible: host.shownKind === "video"
        width: parent.width; height: 320
        radius: 12
        color: T.surface2
        ListView {
            id: videoList
            anchors.fill: parent
            anchors.margins: 6
            clip: true
            model: host.shownSpec.videos || []
            boundsBehavior: Flickable.StopAtBounds
            Tpl.ScrollBar.vertical: NxScrollBar {}
            delegate: Rectangle {
                id: video
                required property var modelData
                required property int index
                width: ListView.view.width
                height: 36
                radius: 8
                color: ListView.isCurrentItem ? T.accent : videoHover.hovered ? T.fill : "transparent"
                Text {
                    x: 10
                    width: parent.width - 20
                    anchors.verticalCenter: parent.verticalCenter
                    text: video.modelData.name
                    color: video.ListView.isCurrentItem ? T.accentInk : T.text
                    font.pixelSize: T.body
                    elide: Text.ElideMiddle
                }
                HoverHandler { id: videoHover; cursorShape: Qt.PointingHandCursor }
                TapHandler {
                    onTapped: videoList.currentIndex = video.index
                    onDoubleTapped: App.dialogs.answer(video.modelData.path)
                }
            }
        }
    }
    Item {
        width: parent.width
        height: 32
        NxButton {
            visible: host.shownKind === "removal"
            text: "取消"
            onClicked: App.dialogs.answer("cancel")
        }
        Row {
            anchors.right: parent.right
            spacing: 8
            // message
            NxButton { visible: host.shownKind === "message"; kind: "primary"; text: "知道了"; onClicked: App.dialogs.answer("") }
            // ask
            NxButton {
                visible: host.shownKind === "ask"
                text: host.shownSpec.rejectIsDefault ? (host.shownSpec.acceptText || "") : (host.shownSpec.rejectText || "")
                onClicked: App.dialogs.answer(host.shownSpec.rejectIsDefault ? "accept" : "reject")
            }
            NxButton {
                visible: host.shownKind === "ask"
                kind: "primary"
                text: host.shownSpec.rejectIsDefault ? (host.shownSpec.rejectText || "") : (host.shownSpec.acceptText || "")
                onClicked: App.dialogs.answer(host.shownSpec.rejectIsDefault ? "reject" : "accept")
            }
            // removal
            NxButton { visible: host.shownKind === "removal"; kind: "danger"; text: "同时删除文件"; onClicked: App.dialogs.answer("delete", remember.checked) }
            NxButton { visible: host.shownKind === "removal"; kind: "primary"; text: "仅移除任务"; onClicked: App.dialogs.answer("keep", remember.checked) }
            // close
            NxButton { visible: host.shownKind === "close"; text: "退出软件"; onClicked: App.dialogs.answer("quit", remember.checked) }
            NxButton { visible: host.shownKind === "close"; kind: "primary"; text: "最小化到托盘"; onClicked: App.dialogs.answer("tray", remember.checked) }
            // video
            NxButton { visible: host.shownKind === "video"; text: "取消"; onClicked: App.dialogs.answer("") }
            NxButton {
                visible: host.shownKind === "video"
                kind: "primary"
                text: "播放"
                onClicked: App.dialogs.answer(host.shownSpec.videos[videoList.currentIndex].path)
            }
        }
    }
}
