// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Window

FocusScope {
    id: card
    required property int subjectId
    required property string title
    required property string meta
    required property string rating
    required property string cover
    property real cardWidth: 182
    // What the cover is decoded for; stays put while the card itself resizes.
    property real decodeWidth: cardWidth
    property real coverRadius: 12
    signal clicked(int subjectId)
    onCoverChanged: Qt.callLater(() => image.shown = image.status === Image.Ready)

    readonly property real posterWidth: cardWidth
    property real posterHeight: Math.round(posterWidth * 1.414)
    width: cardWidth
    height: posterHeight + 70
    activeFocusOnTab: true
    Accessible.role: Accessible.Button
    Accessible.name: title
    Accessible.onPressAction: clicked(subjectId)
    Keys.onReturnPressed: clicked(subjectId)
    Keys.onEnterPressed: clicked(subjectId)
    Keys.onSpacePressed: clicked(subjectId)

    HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
    TapHandler { onTapped: card.clicked(card.subjectId) }

    Rectangle {
        id: poster
        width: card.posterWidth
        height: card.posterHeight
        radius: 12
        color: T.surface3
        scale: hover.hovered && Theme.animated ? 1.03 : 1
        transformOrigin: Item.Center
        Behavior on scale { NumberAnimation { duration: T.normal; easing.type: Easing.OutCubic } }
        Shadow { blur: hover.hovered ? 22 : 10; offset: hover.hovered ? 8 : 3 }
        Text {
            anchors.centerIn: parent
            text: card.cover === "" ? "无封面" : ""
            color: T.text2
            font.pixelSize: T.callout
            font.weight: Font.DemiBold
            visible: image.status !== Image.Ready || image.implicitWidth <= 1
        }
        Image {
            id: image
            anchors.fill: parent
            readonly property real scaleFactor: Screen.devicePixelRatio
            source: card.cover === "" ? "" : "image://cover/" + Math.round(card.coverRadius * scaleFactor) + "/" + encodeURIComponent(card.cover)
            sourceSize: Qt.size(Math.round(card.decodeWidth * scaleFactor), Math.round(card.decodeWidth * 1.414 * scaleFactor))
            asynchronous: true
            cache: true
            // A new size keeps the old picture on screen until it is ready; a new cover
            // in a recycled card must not show the last one.
            retainWhileLoading: true
            property bool shown: false
            onStatusChanged: if (status === Image.Ready) shown = true
            opacity: shown ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: T.fast } }
        }
        Rectangle {
            x: 8; y: 8
            visible: card.rating !== ""
            width: badge.implicitWidth + 14
            height: badge.implicitHeight + 5
            radius: height / 2
            color: Qt.rgba(0, 0, 0, 0.55)
            Text {
                id: badge
                anchors.centerIn: parent
                text: card.rating
                color: "white"
                font.pixelSize: T.footnote
                font.weight: Font.DemiBold
            }
        }
        Rectangle {
            anchors.fill: parent
            anchors.margins: -3
            radius: 15
            color: "transparent"
            border.width: 2
            border.color: T.accent
            visible: card.activeFocus
        }
    }
    Text {
        id: name
        y: poster.height + 10
        width: card.posterWidth
        text: card.title
        color: T.text
        font.pixelSize: T.body
        font.weight: Font.DemiBold
        wrapMode: Text.Wrap
        maximumLineCount: 2
        elide: Text.ElideRight
    }
    Text {
        y: name.y + name.height + 2
        width: card.posterWidth
        text: card.meta
        color: T.text3
        font.pixelSize: T.callout
        elide: Text.ElideRight
    }
}
