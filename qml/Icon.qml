// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Window

Image {
    id: root
    property string name: ""
    property color tint: T.text2
    property real size: 18
    width: size; height: size
    readonly property real scaleFactor: Screen.devicePixelRatio
    sourceSize: Qt.size(Math.ceil(size * scaleFactor), Math.ceil(size * scaleFactor))
    source: name === "" ? "" : "image://icon/" + name + "/" + tint.toString().slice(1)
    cache: true
    smooth: true
}
