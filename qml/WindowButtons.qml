// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only
import QtQuick

// The Windows caption buttons, in their usual place and order, drawn like the other round
// buttons beside them. The close circle shares its centre with the window's rounded corner.
Row {
    id: root
    required property var window
    property bool closeOnly: false
    readonly property int circle: 30
    // With the appearance button's spacing of 6 this makes every gap between circles equal.
    readonly property int cellWidth: 42
    readonly property int cornerRadius: 24
    component Caption: Item {
        id: cell
        property string glyph: ""
        property string tip: ""
        property bool danger: false
        signal activated()
        // Twice the corner radius tall, so the circles sit on its centre line. The last
        // cell runs on to the window's edge: the corner of the screen still closes it.
        width: root.cellWidth + (danger ? root.cornerRadius - root.cellWidth / 2 : 0)
        height: 2 * root.cornerRadius
        Rectangle {
            objectName: cell.danger ? "closeCircle" : ""
            x: (root.cellWidth - width) / 2
            anchors.verticalCenter: parent.verticalCenter
            width: root.circle; height: root.circle
            radius: width / 2
            color: cell.danger && (hover.hovered || tap.pressed)
                ? (tap.pressed ? "#c8423b" : "#e5534b")
                : tap.pressed ? T.fillPressed : hover.hovered ? T.fill : "transparent"
            Behavior on color { ColorAnimation { duration: T.fast } }
        }
        Accessible.role: Accessible.Button
        Accessible.name: tip
        Accessible.onPressAction: activated()
        Canvas {
            id: canvas
            x: (root.cellWidth - width) / 2
            anchors.verticalCenter: parent.verticalCenter
            width: 16; height: 16
            property color ink: cell.danger && hover.hovered ? "white" : T.text
            property string glyph: cell.glyph
            onInkChanged: requestPaint()
            onGlyphChanged: requestPaint()
            onPaint: {
                const g = getContext("2d")
                g.reset()
                g.strokeStyle = ink
                g.lineWidth = 1
                g.beginPath()
                if (glyph === "minimize") { g.moveTo(3, 8.5); g.lineTo(13, 8.5) }
                else if (glyph === "maximize") { g.rect(3.5, 3.5, 9, 9) }
                else if (glyph === "restore") { g.rect(2.5, 5.5, 7, 7); g.moveTo(5.5, 5); g.lineTo(5.5, 2.5); g.lineTo(13.5, 2.5); g.lineTo(13.5, 10.5); g.lineTo(10, 10.5) }
                else { g.moveTo(4, 4); g.lineTo(12, 12); g.moveTo(12, 4); g.lineTo(4, 12) }
                g.stroke()
            }
        }
        HoverHandler { id: hover }
        Tip { text: cell.tip }
        TapHandler { id: tap; onTapped: cell.activated() }
    }
    Caption {
        visible: !root.closeOnly
        glyph: "minimize"; tip: "最小化"
        onActivated: root.window.showMinimized()
    }
    Caption {
        readonly property bool maximized: root.window.visibility === 4
        visible: !root.closeOnly
        glyph: maximized ? "restore" : "maximize"
        tip: maximized ? "还原" : "最大化"
        onActivated: maximized ? root.window.showNormal() : root.window.showMaximized()
    }
    Caption {
        glyph: "close"; tip: "关闭"; danger: true
        onActivated: root.window.close()
    }
}
