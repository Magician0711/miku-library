// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Window

Item {
    id: page
    readonly property var featured: App.home.featured
    Component.onCompleted: App.home.ensureLoaded()

    // Moves with the content, so the colour scrolls away with the card it belongs to.
    Backdrop {
        y: grid.originY - grid.contentY - Shell.band
        cover: page.featured.cover || ""
    }
    PosterGrid {
        id: grid
        anchors.fill: parent
        controller: App.home
        emptyIcon: "calendar"
        header: Component {
            Column {
                x: 0
                width: grid.available
                spacing: 18
                bottomPadding: 20
                Item {
                    // The day's best rated show.
                    id: spotlight
                    visible: page.featured.subjectId !== undefined
                    width: parent.width
                    height: visible ? 214 : 0
                    Rectangle {
                        id: art
                        y: 4
                        width: 146; height: 206
                        radius: 12
                        color: T.surface3
                        Shadow { blur: 22; offset: 8 }
                        Image {
                            anchors.fill: parent
                            readonly property real scaleFactor: Screen.devicePixelRatio
                            source: !page.featured.cover ? "" : "image://cover/" + Math.round(12 * scaleFactor) + "/" + encodeURIComponent(page.featured.cover)
                            sourceSize: Qt.size(Math.round(width * scaleFactor), Math.round(height * scaleFactor))
                            asynchronous: true
                        }
                        HoverHandler { cursorShape: Qt.PointingHandCursor }
                        TapHandler { onTapped: App.home.open(page.featured.subjectId) }
                    }
                    Column {
                        x: art.width + 24
                        anchors.verticalCenter: art.verticalCenter
                        width: Math.min(parent.width - x, 720)
                        spacing: 6
                        Text { text: "今日焦点"; color: T.text2; font.pixelSize: T.callout; font.weight: Font.DemiBold }
                        Text {
                            width: parent.width
                            text: page.featured.title || ""
                            color: T.text
                            font.pixelSize: T.largeTitle
                            font.weight: Font.Bold
                            wrapMode: Text.Wrap
                            maximumLineCount: 2
                            elide: Text.ElideRight
                        }
                        Text { text: page.featured.meta || ""; color: T.text2; font.pixelSize: T.body }
                        Text {
                            width: parent.width
                            text: page.featured.summary || ""
                            visible: text !== ""
                            color: T.text2
                            font.pixelSize: T.body
                            lineHeight: 1.2
                            wrapMode: Text.Wrap
                            maximumLineCount: 2
                            elide: Text.ElideRight
                        }
                        Item { width: 1; height: 4 }
                        NxButton { kind: "primary"; text: "查看资源"; onClicked: App.home.open(page.featured.subjectId) }
                    }
                }
                Item {
                    width: parent.width
                    height: days.height
                    Segmented {
                        id: days
                        Accessible.name: "每周放送日历"
                        items: App.home.days.map(d => ({ text: d.text, value: d.day, tip: d.tip }))
                        value: App.home.day
                        onActivated: value => App.home.showDay(value)
                    }
                }
            }
        }
    }
    Scaffold {
        anchors.fill: parent
        title: "本季放送"
        subtitle: App.home.status
        subtitleTip: App.home.statusTip
        scrolled: grid.scrolled
        toolbar: IconButton { iconName: "refresh"; tip: "刷新放送表"; onClicked: App.home.load() }
    }
}
