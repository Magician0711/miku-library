// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Window

Rectangle {
    id: root
    property string cover: ""
    radius: 14
    color: T.surface3
    Text {
        anchors.centerIn: parent
        text: root.cover === "" ? "无封面" : ""
        color: T.text2
        font.pixelSize: T.callout
        font.weight: Font.DemiBold
        visible: art.status !== Image.Ready || art.implicitWidth <= 1
    }
    Image {
        id: art
        anchors.fill: parent
        readonly property real scaleFactor: Screen.devicePixelRatio
        source: root.cover === "" ? "" : "image://cover/" + Math.round(14 * scaleFactor) + "/" + encodeURIComponent(root.cover)
        // One size per layout, so that dragging the window does not reload it.
        readonly property int decodeWidth: width > 200 ? 340 : 180
        sourceSize: Qt.size(Math.round(decodeWidth * scaleFactor), Math.round(decodeWidth * 1.414 * scaleFactor))
        asynchronous: true
        opacity: status === Image.Ready ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: T.fast } }
    }
}
