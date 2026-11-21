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
    // The controls, the floating navigation and the pointer step aside after three seconds
    // without the pointer moving, and come back with the first movement. They stay while
    // paused, while the pointer is on either bar and while one of the bar's menus is open.
    property bool controlsWanted: true
    readonly property bool navHovered: host !== null && host.pointerOnNav === true
    readonly property bool controlsShown: playing && (controlsWanted || player.paused || barHover.hovered
                                                      || navHovered || bar.menuOpen || bar.activeFocus)
    // Where the pointer was at its last real movement. While the video plays, Qt delivers
    // hover again every frame with the pointer standing still; that must not count.
    property point lastPointer: Qt.point(-1, -1)
    property int restoreVisibility: Window.Windowed

    function poke() {
        controlsWanted = true
        idle.restart()
    }
    function pointerAt(point) {
        if (point.x === lastPointer.x && point.y === lastPointer.y)
            return
        lastPointer = point
        poke()
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
    Timer { id: idle; objectName: "playerIdle"; interval: 3000; onTriggered: page.controlsWanted = false }
    onNavHoveredChanged: poke()
    // The window hides its floating navigation while the player rests.
    Binding {
        when: page.host !== null
        target: page.host
        property: "playerResting"
        value: page.shown && page.playing && !page.controlsShown
    }
    HoverHandler {
        objectName: "playerHover"
        cursorShape: page.playing && !page.controlsShown ? Qt.BlankCursor : Qt.ArrowCursor
        onPointChanged: page.pointerAt(point.scenePosition)
    }

    Keys.onSpacePressed: { player.togglePause(); poke() }
    Keys.onLeftPressed: { player.seekBy(-5); poke() }
    Keys.onRightPressed: { player.seekBy(5); poke() }
    Keys.onUpPressed: { player.setVolume(player.volume + 5); poke() }
    Keys.onDownPressed: { player.setVolume(player.volume - 5); poke() }
    Keys.onPressed: event => {
        if (event.key === Qt.Key_F) { toggleFull(); event.accepted = true }
        else if (event.key === Qt.Key_I) { player.setInterpolation(player.interpolation === 0 ? 1 : 0); poke(); event.accepted = true }
        else if (event.key === Qt.Key_M) { player.toggleMute(); poke(); event.accepted = true }
        else if (event.key === Qt.Key_Escape && full) { toggleFull(); event.accepted = true }
    }

    // A playing picture gets a plain page around it, not the desktop showing through.
    Rectangle { anchors.fill: parent; color: T.bg; visible: page.playing }

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
        // Above the floating navigation, which would otherwise cover the picture's controls.
        height: parent.height - y - (page.full ? 0 : Shell.trailing + Shell.bottomInset)

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
            readonly property bool menuOpen: audioMenu.popup.visible || subtitleMenu.popup.visible
                                             || interpolationMenu.popup.visible || upscaleMenu.popup.visible
            HoverHandler { id: barHover }

            Row {
                id: left
                x: 10
                anchors.verticalCenter: parent.verticalCenter
                spacing: 6
                IconButton {
                    anchors.verticalCenter: parent.verticalCenter
                    objectName: "playerPause"
                    iconName: page.player.paused ? "play" : "pause"
                    tip: page.player.paused ? qsTr("播放 (空格)") : qsTr("暂停 (空格)")
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
                name: qsTr("播放进度")
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
                    tip: page.player.muted ? qsTr("取消静音 (M)") : qsTr("静音 (M)")
                    onClicked: page.player.toggleMute()
                }
                NxSeek {
                    id: volumeBar
                    name: qsTr("音量")
                    anchors.verticalCenter: parent.verticalCenter
                    visible: bar.width > 760
                    width: 76
                    from: 0; to: 100; stepSize: 5
                    onMoved: page.player.setVolume(value)
                    Binding on value { when: !volumeBar.pressed; value: page.player.volume }
                }
                NxSelect {
                    id: audioMenu
                    anchors.verticalCenter: parent.verticalCenter
                    visible: page.player.audioTracks.length > 1
                    implicitWidth: 100 * Theme.textScale
                    popupWidth: 220
                    tip: qsTr("音轨")
                    model: page.player.audioTracks
                    value: page.player.audioTrack
                    onChosen: value => page.player.setAudioTrack(value)
                }
                NxSelect {
                    id: subtitleMenu
                    anchors.verticalCenter: parent.verticalCenter
                    visible: page.player.subtitleTracks.length > 1
                    implicitWidth: 100 * Theme.textScale
                    popupWidth: 220
                    tip: qsTr("字幕")
                    model: page.player.subtitleTracks
                    value: page.player.subtitleTrack
                    onChosen: value => page.player.setSubtitleTrack(value)
                }
                NxSelect {
                    id: interpolationMenu
                    objectName: "playerInterpolation"
                    anchors.verticalCenter: parent.verticalCenter
                    visible: bar.width > 680
                    implicitWidth: Math.ceil(interpolationLabel.advanceWidth) + leftPadding + rightPadding + 4
                    TextMetrics { id: interpolationLabel; font.pixelSize: T.body; text: qsTr("补帧：运动补偿") }
                    popupWidth: 260
                    tip: page.player.motionAvailable
                         ? qsTr("补帧让画面更流畅：平滑模式在相邻帧之间过渡；运动补偿按画面运动生成中间帧，需要较强的 CPU")
                         : qsTr("补帧让画面更流畅：平滑模式在相邻帧之间过渡。安装 VapourSynth 和 MVTools 后可使用运动补偿")
                    model: {
                        const choices = [{ text: qsTr("补帧：关闭"), value: 0 },
                                         { text: qsTr("补帧：平滑"), value: 1, note: qsTr("帧混合，几乎不占资源") }]
                        if (page.player.motionAvailable)
                            choices.push({ text: qsTr("补帧：运动补偿"), value: 2, note: qsTr("VapourSynth · MVTools，补到屏幕刷新率") })
                        return choices
                    }
                    value: page.player.interpolation
                    onChosen: value => page.player.setInterpolation(value)
                }
                NxSelect {
                    id: upscaleMenu
                    objectName: "playerUpscale"
                    anchors.verticalCenter: parent.verticalCenter
                    // Wide enough for the longest choice, at any text size.
                    implicitWidth: Math.ceil(upscaleLabel.advanceWidth) + leftPadding + rightPadding + 4
                    TextMetrics { id: upscaleLabel; font.pixelSize: T.body; text: qsTr("超分：高画质") }
                    popupWidth: 200
                    tip: qsTr("Anime4K 实时超分：高画质需要较强的显卡，卡顿时请调低")
                    model: [{ text: qsTr("超分：关闭"), value: 0 }, { text: qsTr("超分：流畅"), value: 1 }, { text: qsTr("超分：高画质"), value: 2 }]
                    value: page.player.upscale
                    onChosen: value => page.player.setUpscale(value)
                }
                IconButton {
                    anchors.verticalCenter: parent.verticalCenter
                    iconName: page.full ? "exit-fullscreen" : "fullscreen"
                    tip: page.full ? qsTr("退出全屏 (Esc)") : qsTr("全屏 (F)")
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
        title: qsTr("现在没有播放视频")
        description: page.player.available ? qsTr("在番剧详情里点「播放」，视频会在这里打开。离开这一页也会继续播放。") : qsTr("没有找到内置播放器组件，视频会用系统默认播放器打开。")
        actionText: qsTr("打开我的追番")
        onAction: App.selectPage(2)
    }
}
