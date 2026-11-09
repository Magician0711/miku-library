// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Templates as Tpl

// One pop-up button for filters and preferences. Items are {text, value}, and may add
// a note shown at the far end of their row.
Tpl.ComboBox {
    id: c
    property var value
    property string tip: ""
    property real popupWidth: 170
    signal chosen(var value)

    textRole: "text"
    valueRole: "value"
    implicitWidth: 180
    implicitHeight: 32
    leftPadding: 14; rightPadding: 30
    hoverEnabled: true
    focusPolicy: Qt.TabFocus
    Accessible.name: tip !== "" ? tip : displayText
    HoverHandler { cursorShape: Qt.PointingHandCursor }
    Tip { text: c.tip }

    function sync() {
        const index = indexOfValue(value)
        if (index >= 0 && index !== currentIndex) currentIndex = index
    }
    onValueChanged: sync()
    onModelChanged: Qt.callLater(sync)
    Component.onCompleted: sync()
    onActivated: index => c.chosen(valueAt(index))

    background: Rectangle {
        radius: height / 2
        color: c.pressed || c.popup.visible ? T.fillPressed : c.hovered && c.enabled ? T.fillHover : T.fill
        Behavior on color { ColorAnimation { duration: T.fast } }
        FocusRing { target: c; corner: parent.radius }
    }
    contentItem: Text {
        text: c.displayText
        color: c.enabled ? T.text : T.disabledText
        font.pixelSize: T.body
        elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter
    }
    indicator: Icon {
        x: c.width - 25; y: (c.height - 15) / 2
        size: 15
        name: "selector"
        tint: c.enabled ? T.text2 : T.disabledText
    }
    delegate: Tpl.ItemDelegate {
        id: row
        required property int index
        required property var modelData
        width: ListView.view ? ListView.view.width : 0
        implicitHeight: 32
        hoverEnabled: true
        highlighted: c.highlightedIndex === index
        readonly property bool current: c.currentIndex === index
        readonly property bool lit: row.hovered || row.highlighted
        leftPadding: 30; rightPadding: 12
        HoverHandler { cursorShape: Qt.PointingHandCursor }
        background: Rectangle {
            anchors.fill: parent
            radius: 7
            color: row.lit ? T.accent : "transparent"
        }
        contentItem: Item {
            Text {
                width: parent.width - (note.visible ? note.implicitWidth + 12 : 0)
                height: parent.height
                text: row.modelData.text !== undefined ? row.modelData.text : row.modelData
                color: row.lit ? T.accentInk : T.text
                font.pixelSize: T.body
                elide: Text.ElideRight
                verticalAlignment: Text.AlignVCenter
            }
            Text {
                id: note
                anchors.right: parent.right
                height: parent.height
                visible: text !== ""
                text: row.modelData.note || ""
                color: row.lit ? T.accentInk : T.text3
                font.pixelSize: T.callout
                verticalAlignment: Text.AlignVCenter
            }
        }
        Icon { x: 9; y: (row.height - 14) / 2; size: 14; name: "check"; tint: row.lit ? T.accentInk : T.text; visible: row.current }
    }
    popup: Tpl.Popup {
        y: c.height + 5
        width: Math.max(c.width, c.popupWidth)
        implicitHeight: Math.min(list.contentHeight + 10, 340)
        padding: 5
        // Stays inside the window, also for a button at its right edge.
        margins: 10
        modal: false
        focus: true
        closePolicy: Tpl.Popup.CloseOnEscape | Tpl.Popup.CloseOnPressOutside
        enter: Transition {
            ParallelAnimation {
                NumberAnimation { property: "opacity"; from: 0; to: 1; duration: T.fast; easing.type: Easing.OutCubic }
                NumberAnimation { property: "scale"; from: 0.97; to: 1; duration: T.fast; easing.type: Easing.OutCubic }
            }
        }
        exit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: T.fast } }
        background: Rectangle {
            radius: 12
            color: T.surface
            border.color: T.border
            Shadow { blur: 18; offset: 6 }
        }
        contentItem: ListView {
            id: list
            clip: true
            model: c.delegateModel
            currentIndex: c.highlightedIndex
            boundsBehavior: Flickable.StopAtBounds
            Tpl.ScrollBar.vertical: NxScrollBar {}
        }
    }
}
