// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only
import QtQuick

Scaffold {
    id: page
    title: "我的追番"
    subtitle: App.library.status
    scrolled: posters.scrolled
    readonly property bool shown: enabled
    onShownChanged: App.library.active = shown
    Component.onCompleted: App.library.active = true
    toolbar: [
        Segmented {
            items: [{ text: "全部", value: 0 }, { text: "追番", value: 1 }, { text: "已下载", value: 2 }]
            value: App.library.scope
            onActivated: value => App.library.scope = value
        },
        NxButton {
            text: App.library.checking ? "检查中…" : "检查追更"
            iconName: "refresh"
            busy: App.library.checking
            enabled: !App.library.checking
            tip: "立即检查下载过的番剧在同一字幕组是否有新集"
            onClicked: App.library.checkUpdatesRequested()
        }
    ]
    PosterGrid {
        id: posters
        anchors.fill: parent
        controller: App.library
        emptyIcon: "library"
        emptyActionText: "搜索番剧"
        onEmptyAction: App.selectPage(1)
    }
}
