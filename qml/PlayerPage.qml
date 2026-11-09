// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Window
import MikuLibrary.Native

// The picture fills the page; the controls float over it and step aside while watching.
FocusScope {
    id: page
    readonly property var player: App.player
    readonly property var host: Window.window
    readonly property bool shown: enabled
    readonly property bool full: host !== null && host.visibility === Window.FullScreen
    readonly property bool playing: player.active
    property bool controlsWanted: true
    readonly property bool controlsShown: playing && (controlsWanted || player.paused || bar.activeFocus)
    property int restoreVisibility: Window.Windowed

    function poke() {
        controlsWanted = true
        idle.restart()
    }
    function toggleFull() {
        if (full) {
            restoreVisibility === Window.Maximized ? host.showMaximized() : host.showNormal()
        } else {
            restoreVisibility = host.visibility === Window.Maximized ? Window.Maximized : Window.Windowed
            host.showFullScreen()
        }
    }
    function clock(seconds) {
        const total = Math.max(0, Math.floor(seconds))
        const hours = Math.floor(total / 3600)
        const minutes = Math.floor(total / 60) % 60
        const rest = String(total % 60).padStart(2, "0")
        return hours > 0 ? hours + ":" + String(minutes).padStart(2, "0") + ":" + rest : minutes + ":" + rest
    }

    onShownChanged: if (shown) { forceActiveFocus(); poke() }
    Component.onCompleted: if (shown) forceActiveFocus()
    Binding { target: page.player; property: "videoVisible"; value: page.shown && page.host !== null && page.host.visibility !== Window.Minimized && page.host.visible }
    Timer { id: idle; interval: 3000; onTriggered: page.controlsWanted = false }

    Keys.onSpacePressed: { player.togglePause(); poke() }
    Keys.onLeftPressed: { player.seekBy(-5); poke() }
    Keys.onRightPressed: { player.seekBy(5); poke() }
    Keys.onUpPressed: { player.setVolume(player.volume + 5); poke() }
    Keys.onDownPressed: { player.setVolume(player.volume - 5); poke() }
    Keys.onPressed: event => {
        if (event.key === Qt.Key_F) { toggleFull(); event.accepted = true }
        else if (event.key === Qt.Key_M) { player.toggleMute(); poke(); event.accepted = true }
        else if (event.key === Qt.Key_Escape && full) { toggleFull(); event.accepted = true }
    }

    Rectangle { anchors.fill: parent; color: T.bg }

    Column {
        visible: !page.full && page.playing
        x: Shell.leading
        y: 12
        width: parent.width - x - Shell.caption - 16
        spacing: 1
        Text {
            width: parent.width
            text: page.player.title
            color: T.text
            font.pixelSize: T.title3
            font.weight: Font.DemiBold
            elide: Text.ElideRight
        }
        Text {
            width: parent.width
            text: page.player.subtitle
            color: T.text3
            font.pixelSize: T.callout
            elide: Text.ElideRight
        }
    }

    Item {
        id: stage
        visible: page.playing
        x: page.full ? 0 : Shell.leading
        y: page.full ? 0 : Shell.toolBand
        width: parent.width - x - (page.full ? 0 : Shell.trailing)
        height: parent.height - y - (page.full ? 0 : Shell.trailing)

        Rectangle { anchors.fill: parent; color: "black" }
        MpvVideo {
            objectName: "playerVideo"
            anchors.fill: parent
            player: page.player
        }
        // The page colour, cut out in the shape of the rounded picture.
        Rectangle {
            visible: !page.full
            anchors.fill: parent
            anchors.margins: -10
            radius: 24
            color: "transparent"
            border.width: 10
            border.color: T.bg
        }

        HoverHandler {
            cursorShape: page.controlsShown || !page.full ? Qt.ArrowCursor : Qt.BlankCursor
            onPointChanged: page.poke()
        }
        TapHandler {
            onSingleTapped: { page.player.togglePause(); page.poke(); page.forceActiveFocus() }
            onDoubleTapped: page.toggleFull()
        }

        Rectangle {
            id: bar
            objectName: "playerControls"
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 16
            width: Math.min(parent.width - 32, 1040)
            height: Math.round(52 * Theme.textScale)
            radius: 16
            color: T.surface
            border.width: 1
            border.color: T.border
            opacity: page.controlsShown ? 0.97 : 0
            visible: opacity > 0
            Behavior on opacity { NumberAnimation { duration: T.normal; easing.type: Easing.OutCubic } }
            HoverHandler { onPointChanged: page.poke() }

            Row {
                id: left
                x: 10
                anchors.verticalCenter: parent.verticalCenter
                spacing: 6
                IconButton {
                    anchors.verticalCenter: parent.verticalCenter
                    objectName: "playerPause"
                    iconName: page.player.paused ? "play" : "pause"
                    tip: page.player.paused ? "播放 (空格)" : "暂停 (空格)"
                    onClicked: page.player.togglePause()
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: page.clock(page.player.position)
                    color: T.text
                    font.pixelSize: T.callout
                    font.features: { "tnum": 1 }
                }
            }
            NxSeek {
                id: seekBar
                objectName: "playerSeek"
                name: "播放进度"
                x: left.x + left.width + 10
                anchors.verticalCenter: parent.verticalCenter
                width: right.x - x - 10
                from: 0
                to: Math.max(1, page.player.duration)
                stepSize: 5
                onMoved: page.player.seek(value)
                Binding on value { when: !seekBar.pressed; value: page.player.position }
            }
            Row {
                id: right
                anchors.right: parent.right
                anchors.rightMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                spacing: 6
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: page.clock(page.player.duration)
                    color: T.text3
                    font.pixelSize: T.callout
                    font.features: { "tnum": 1 }
                }
                IconButton {
                    anchors.verticalCenter: parent.verticalCenter
                    iconName: page.player.muted || page.player.volume === 0 ? "mute" : "volume"
                    tip: page.player.muted ? "取消静音 (M)" : "静音 (M)"
                    onClicked: page.player.toggleMute()
                }
                NxSeek {
                    id: volumeBar
                    name: "音量"
                    anchors.verticalCenter: parent.verticalCenter
                    visible: bar.width > 760
                    width: 76
                    from: 0; to: 100; stepSize: 5
                    onMoved: page.player.setVolume(value)
                    Binding on value { when: !volumeBar.pressed; value: page.player.volume }
                }
                NxSelect {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: page.player.audioTracks.length > 1
                    implicitWidth: 100 * Theme.textScale
                    popupWidth: 220
                    tip: "音轨"
                    model: page.player.audioTracks
                    value: page.player.audioTrack
                    onChosen: value => page.player.setAudioTrack(value)
                }
                NxSelect {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: page.player.subtitleTracks.length > 1
                    implicitWidth: 100 * Theme.textScale
                    popupWidth: 220
                    tip: "字幕"
                    model: page.player.subtitleTracks
                    value: page.player.subtitleTrack
                    onChosen: value => page.player.setSubtitleTrack(value)
                }
                NxSelect {
                    objectName: "playerUpscale"
                    anchors.verticalCenter: parent.verticalCenter
                    implicitWidth: 116 * Theme.textScale
                    popupWidth: 200
                    tip: "Anime4K 实时超分：高画质需要较强的显卡，卡顿时请调低"
                    model: [{ text: "超分：关闭", value: 0 }, { text: "超分：流畅", value: 1 }, { text: "超分：高画质", value: 2 }]
                    value: page.player.upscale
                    onChosen: value => page.player.setUpscale(value)
                }
                IconButton {
                    anchors.verticalCenter: parent.verticalCenter
                    iconName: page.full ? "exit-fullscreen" : "fullscreen"
                    tip: page.full ? "退出全屏 (Esc)" : "全屏 (F)"
                    onClicked: page.toggleFull()
                }
            }
        }
    }

    EmptyState {
        anchors.fill: parent
        anchors.leftMargin: Shell.leading
        anchors.rightMargin: Shell.trailing
        iconName: "play"
        shown: !page.playing
        title: "现在没有播放视频"
        description: page.player.available ? "在番剧详情里点「播放」，视频会在这里打开。离开这一页也会继续播放。" : "没有找到内置播放器组件，视频会用系统默认播放器打开。"
        actionText: "打开我的追番"
        onAction: App.selectPage(2)
    }
}
