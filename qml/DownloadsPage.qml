// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Templates as Tpl

Scaffold {
    id: page
    title: qsTr("下载任务")
    subtitle: dl.summary + " · " + App.transfer
    subtitleTip: dl.engineVersion
    readonly property int metricHeight: Math.round(64 * Theme.textScale)
    fixedHeight: 54 + metricHeight
    readonly property var dl: App.downloads
    readonly property bool shown: enabled
    onShownChanged: dl.active = shown
    Component.onCompleted: dl.active = true

    toolbar: [
        NxButton {
            id: backendButton
            text: page.dl.backend
            iconName: "download"
            tip: qsTr("切换新任务使用的下载器")
            onClicked: window.chooseDownloader(backendButton)
        },
        NxButton { text: qsTr("打开下载目录"); iconName: "folder"; onClicked: App.openDownloadFolder() }
    ]

    component Metric: Group {
        id: metric
        property string caption
        property string value
        property string glyph
        height: page.metricHeight
        Icon { x: 16; anchors.verticalCenter: parent.verticalCenter; size: 22; name: metric.glyph; tint: T.text3 }
        Column {
            x: 50
            anchors.verticalCenter: parent.verticalCenter
            Text { text: metric.caption; color: T.text3; font.pixelSize: T.callout }
            Text { text: metric.value; color: T.text; font.pixelSize: T.title1; font.weight: Font.DemiBold }
        }
    }
    readonly property real contentWidth: width - leading - trailing
    // Columns give way on narrow windows; the task column takes what is left.
    readonly property bool roomy: restingWidth >= 860
    readonly property int checkWidth: 48
    // Wide windows spread the figures too, instead of giving it all to the task name.
    readonly property real spread: Math.max(1, contentWidth / 1300)
    readonly property int progressWidth: (roomy ? 200 : 140) * spread
    readonly property int speedWidth: 104 * spread
    readonly property int sizeWidth: roomy ? 88 * spread : 0
    readonly property int peersWidth: roomy ? 84 * spread : 0
    readonly property int etaWidth: roomy ? 92 * spread : 0
    readonly property int taskWidth: contentWidth - checkWidth - progressWidth - speedWidth - sizeWidth - peersWidth - etaWidth

    fixed: Item {
        anchors.fill: parent
        Row {
            x: page.leading; y: Shell.band + 2
            width: page.contentWidth
            spacing: 12
            Metric { width: (parent.width - 36) / 4; caption: qsTr("进行中"); value: page.dl.activeCount; glyph: "pulse" }
            Metric { width: (parent.width - 36) / 4; caption: qsTr("下载速度"); value: page.dl.downSpeed; glyph: "download" }
            Metric { width: (parent.width - 36) / 4; caption: qsTr("上传速度"); value: page.dl.upSpeed; glyph: "export" }
            Metric {
                width: (parent.width - 36) / 4
                caption: qsTr("剩余 · %1").arg(page.dl.remainingTime)
                value: page.dl.remaining
                glyph: "clock"
                Tip { text: qsTr("所有未完成任务还需下载的大小，以及按当前速度估算的时间") }
            }
        }
        Item {
            x: page.leading; y: Shell.band + 12 + page.metricHeight
            width: page.contentWidth; height: 32
            Segmented {
                id: filterBar
                objectName: "downloadFilter"
                Accessible.name: qsTr("按状态筛选任务")
                anchors.verticalCenter: parent.verticalCenter
                visible: list.count > 0 || page.dl.filter !== 0
                compact: true
                items: page.dl.filters
                value: page.dl.filter
                onActivated: value => page.dl.filter = value
            }
            Text {
                x: filterBar.visible ? filterBar.width + 12 : 0
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - actions.width - 12 - x
                text: page.dl.hint
                visible: list.count > 0
                color: T.text3
                font.pixelSize: T.callout
                elide: Text.ElideRight
            }
            Row {
                id: actions
                anchors.right: parent.right
                spacing: 8
                NxButton { text: qsTr("打开下载器"); iconName: "external"; visible: page.dl.canOpenClient; onClicked: page.dl.openClient() }
                NxButton { text: page.dl.pauseText; iconName: "pause"; enabled: page.dl.canControl; onClicked: page.dl.pauseSelected() }
                NxButton { text: page.dl.resumeText; iconName: page.dl.retrying ? "refresh" : "play"; enabled: page.dl.canControl; onClicked: page.dl.resumeSelected() }
                NxButton { text: page.dl.removeText; iconName: "trash"; kind: "danger"; enabled: page.dl.selectedCount > 0; onClicked: page.dl.removeSelected() }
            }
        }
    }

    // The group ends with its last task, so a short list leaves no empty frame.
    Group {
        id: table
        x: page.leading; y: page.contentTop + 4
        width: page.contentWidth
        // Ends above the floating navigation.
        height: Math.min(parent.height - y - 20 - Shell.bottomInset, 38 + Math.max(list.contentHeight, 1))
        visible: list.count > 0
        clip: true
        component Head: Text {
            height: 38
            leftPadding: 12
            verticalAlignment: Text.AlignVCenter
            color: T.text3
            font.pixelSize: T.callout
            font.weight: Font.DemiBold
            clip: true
        }
        Row {
            id: head
            Head {
                width: page.checkWidth
                text: page.dl.allSelected ? qsTr("取消") : qsTr("全选")
                HoverHandler { cursorShape: Qt.PointingHandCursor }
                Tip { text: qsTr("点击全选或取消全选") }
                TapHandler { onTapped: page.dl.toggleAll() }
            }
            Head { width: page.taskWidth; text: qsTr("任务") }
            Head { width: page.progressWidth; text: qsTr("进度") }
            Head { width: page.speedWidth; text: qsTr("速度") }
            Head { width: page.sizeWidth; visible: page.roomy; text: qsTr("大小") }
            Head { width: page.peersWidth; visible: page.roomy; text: qsTr("做种 / 连接") }
            Head { width: page.etaWidth; visible: page.roomy; text: qsTr("剩余时间") }
        }
        Divider { y: 37; width: parent.width }
        ListView {
            id: list
            y: 38
            width: parent.width
            height: parent.height - y
            model: page.dl.model
            clip: true
            interactive: false
            boundsBehavior: Flickable.StopAtBounds
            reuseItems: true
            Tpl.ScrollBar.vertical: NxScrollBar {}
            SmoothWheel { view: list }
            delegate: Rectangle {
                id: row
                required property int index
                required property string key
                required property string title
                required property string status
                required property string tone
                required property real progress
                required property string progressText
                required property string down
                required property string up
                required property string size
                required property string peers
                required property string eta
                required property string path
                required property bool selected
                required property bool sequential
                width: ListView.view.width
                height: 54
                color: selected ? T.accentSoft : rowHover.hovered ? T.surface2 : "transparent"
                HoverHandler { id: rowHover }
                Tip { text: row.title + "\n" + row.status }
                TapHandler {
                    acceptedButtons: Qt.LeftButton
                    acceptedModifiers: Qt.NoModifier
                    onTapped: eventPoint => {
                        if (eventPoint.position.x < page.checkWidth) page.dl.toggle(row.key)
                        else page.dl.selectOnly(row.key)
                    }
                    // Straight to the finished episodes; otherwise to the files on disk.
                    onDoubleTapped: eventPoint => {
                        if (eventPoint.position.x < page.checkWidth) return
                        if (page.dl.canPlay(row.key)) page.dl.play(row.key)
                        else if (page.dl.folderExists(row.path)) page.dl.openFolder(row.path)
                    }
                }
                TapHandler { acceptedButtons: Qt.LeftButton; acceptedModifiers: Qt.ControlModifier; onTapped: page.dl.toggle(row.key) }
                TapHandler { acceptedButtons: Qt.LeftButton; acceptedModifiers: Qt.ShiftModifier; onTapped: page.dl.extendTo(row.key) }
                TapHandler {
                    acceptedButtons: Qt.RightButton
                    onTapped: {
                        page.dl.prepareMenu(row.key)
                        contextMenu.title_ = row.title
                        contextMenu.path = row.path
                        contextMenu.key = row.key
                        contextMenu.playable = page.dl.canPlay(row.key)
                        contextMenu.popup(row)
                    }
                }
                NxCheck {
                    x: 15
                    anchors.verticalCenter: parent.verticalCenter
                    checked: row.selected
                    focusPolicy: Qt.TabFocus
                    onToggled: { checked = Qt.binding(() => row.selected); page.dl.toggle(row.key) }
                }
                Column {
                    x: page.checkWidth + 12
                    anchors.verticalCenter: parent.verticalCenter
                    width: page.taskWidth - 24
                    spacing: 2
                    Text { width: parent.width; text: row.title; color: T.text; font.pixelSize: T.body; font.weight: Font.Medium; elide: Text.ElideMiddle }
                    Text { width: parent.width; text: row.sequential ? qsTr("%1 · 按顺序下载").arg(row.status) : row.status; color: T.tone(row.tone); font.pixelSize: T.callout; elide: Text.ElideRight }
                }
                Item {
                    x: page.checkWidth + page.taskWidth + 12
                    width: page.progressWidth - 24
                    height: parent.height
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        visible: row.progress < 0
                        text: qsTr("由外部管理")
                        color: T.text3
                        font.pixelSize: T.callout
                    }
                    Rectangle {
                        visible: row.progress >= 0
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width - 46; height: 5; radius: 2.5
                        color: T.fill
                        Rectangle {
                            height: parent.height; radius: 2.5
                            width: parent.width * Math.max(0, Math.min(100, row.progress)) / 100
                            color: row.progress >= 100 ? T.ok : row.tone === "muted" ? T.borderStrong : T.accent
                            Behavior on width { NumberAnimation { duration: T.slow; easing.type: Easing.OutCubic } }
                        }
                    }
                    Text {
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        visible: row.progress >= 0
                        text: row.progressText
                        color: T.text2
                        font.pixelSize: T.callout
                    }
                }
                Column {
                    x: page.checkWidth + page.taskWidth + page.progressWidth + 12
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 2
                    Text { text: row.down; color: T.text; font.pixelSize: T.callout }
                    Text { text: row.up; color: T.text3; font.pixelSize: T.callout; visible: text !== "" }
                }
                component Cell: Text {
                    anchors.verticalCenter: parent.verticalCenter
                    color: T.text2
                    font.pixelSize: T.callout
                    visible: page.roomy
                }
                Cell { x: page.checkWidth + page.taskWidth + page.progressWidth + page.speedWidth + 12; text: row.size }
                Cell { x: page.checkWidth + page.taskWidth + page.progressWidth + page.speedWidth + page.sizeWidth + 12; text: row.peers }
                Cell { x: page.checkWidth + page.taskWidth + page.progressWidth + page.speedWidth + page.sizeWidth + page.peersWidth + 12; text: row.eta }
                Divider { x: page.checkWidth + 12; y: 0; width: parent.width - x; visible: row.index > 0 }
            }
        }
    }
    EmptyState {
        x: page.leading; y: page.contentTop
        width: page.contentWidth
        height: parent.height - y - 40
        iconName: "download"
        title: qsTr("还没有下载任务")
        description: qsTr("在番剧详情中选择资源，点击「下载」开始传输。")
        actionText: qsTr("搜索番剧")
        onAction: App.selectPage(1)
        shown: list.count === 0
    }
    NxMenu {
        id: contextMenu
        property string title_: ""
        property string path: ""
        property string key: ""
        property bool playable: false
        NxMenuItem { text: qsTr("播放已完成的视频"); iconName: "play"; enabled: contextMenu.playable; onTriggered: page.dl.play(contextMenu.key) }
        NxMenuItem { text: page.dl.pauseText; iconName: "pause"; enabled: page.dl.canControl; onTriggered: page.dl.pauseSelected() }
        NxMenuItem { text: page.dl.resumeText; iconName: "refresh"; enabled: page.dl.canControl; onTriggered: page.dl.resumeSelected() }
        NxMenuItem {
            text: qsTr("按顺序下载（先完成前面的集数）")
            implicitWidth: 280
            checkable: true
            checked: page.dl.sequentialChecked
            enabled: !contextMenu.key.startsWith("external:")
            onTriggered: page.dl.toggleSequential()
        }
        NxMenuItem { text: qsTr("打开所在文件夹"); iconName: "folder"; enabled: page.dl.folderExists(contextMenu.path); onTriggered: page.dl.openFolder(contextMenu.path) }
        NxMenuItem { text: qsTr("复制任务名称"); iconName: "copy"; onTriggered: page.dl.copyTitle(contextMenu.title_) }
        NxMenuItem { text: page.dl.removeText; iconName: "trash"; tint: T.err; onTriggered: page.dl.removeSelected() }
    }
}
