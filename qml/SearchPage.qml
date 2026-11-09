// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only
import QtQuick

Scaffold {
    id: page
    title: "搜索番剧"
    subtitle: App.search.status
    scrolled: posters.scrolled
    fixedHeight: 56
    property double composedAt: 0
    function submit() {
        // Enter that confirms an IME candidate is not a search.
        if (input.inputMethodComposing || Date.now() - composedAt < 60)
            return
        App.search.search(input.text)
    }
    Connections {
        target: App
        function onFocusSearchRequested() {
            posters.contentY = posters.originY - posters.topInset
            input.forceActiveFocus()
            input.selectAll()
        }
    }
    Connections {
        target: App.search
        function onQueryChanged() { if (input.text.trim() !== App.search.query) input.text = App.search.query }
    }
    Component.onCompleted: { input.text = App.search.query; input.forceActiveFocus() }

    PosterGrid {
        id: posters
        anchors.fill: parent
        controller: App.search
        emptyIcon: "search"
        topInset: page.contentTop + 4
    }
    fixed: Item {
        x: page.leading
        y: Shell.band + 2 - page.lift
        width: parent.width - page.leading - page.trailing
        height: 44
        Rectangle {
            id: box
            width: parent.width - sort.width - 16
            height: 44
            radius: 22
            color: T.surface
            border.width: input.activeFocus ? 2 : 0
            border.color: T.accent
            Shadow { blur: 8; offset: 2 }
            Icon { x: 16; y: 12; size: 20; name: "search"; tint: T.text3 }
            NxField {
                id: input
                x: 44; y: 2
                width: parent.width - 52
                height: 40
                plain: true
                clearable: true
                placeholderText: "输入中文、日文或英文番名，按回车搜索"
                Accessible.name: "番剧名称"
                Component.onCompleted: font.pixelSize = 15
                onAccepted: page.submit()
                onInputMethodComposingChanged: {
                    if (!inputMethodComposing) {
                        page.composedAt = Date.now()
                        if (text.trim() === "") App.search.reset()
                    }
                }
                onTextChanged: if (text.trim() === "" && !inputMethodComposing) App.search.reset()
            }
        }
        // Order of the results; the default favours recent, well rated anime.
        Segmented {
            id: sort
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            items: [
                { text: "最新", value: 0, tip: "按播出时间从新到旧，同一年份评分高的在前" },
                { text: "评分", value: 1, tip: "按 Bangumi 评分从高到低" },
                { text: "相关度", value: 2, tip: "按与搜索词的匹配程度" }
            ]
            value: App.search.sort
            onActivated: value => App.search.sort = value
        }
    }
}
