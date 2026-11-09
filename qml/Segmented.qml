// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only
import QtQuick

// Items are {text, value, icon?, tip?}. The thumb glides to the chosen one.
Rectangle {
    id: root
    property var items: []
    property var value
    property bool compact: false
    signal activated(var value)

    readonly property int inset: 2
    readonly property var chosen: thumb.current ? thumb.current.modelData.value : undefined
    radius: height / 2
    color: T.fill
    implicitWidth: row.implicitWidth + inset * 2
    implicitHeight: (compact ? 26 : 30) + inset * 2
    activeFocusOnTab: true
    readonly property bool visualFocus: activeFocus
    function step(by) {
        const values = items.map(item => item.value)
        const next = values.indexOf(value) + by
        if (next >= 0 && next < values.length)
            activated(values[next])
    }
    Keys.onLeftPressed: step(-1)
    Keys.onRightPressed: step(1)
    FocusRing { target: root; corner: root.radius }

    Rectangle {
        id: thumb
        property Item current: null
        function update() {
            for (let i = 0; i < repeater.count; ++i) {
                const item = repeater.itemAt(i)
                // Not item.selected: that binding may not have caught up with the value yet.
                if (item && item.modelData.value === root.value) { current = item; return }
            }
            current = null
        }
        visible: current !== null
        x: current ? current.x + root.inset : 0
        y: root.inset
        width: current ? current.width : 0
        height: root.height - root.inset * 2
        radius: height / 2
        color: T.dark ? "#55555c" : "#ffffff"
        Shadow { blur: 3; offset: 1 }
        Behavior on x { enabled: thumb.visible; NumberAnimation { duration: T.normal; easing.type: Easing.OutCubic } }
        Behavior on width { enabled: thumb.visible; NumberAnimation { duration: T.normal; easing.type: Easing.OutCubic } }
    }
    onValueChanged: thumb.update()
    onItemsChanged: Qt.callLater(thumb.update)
    Row {
        id: row
        x: root.inset; y: root.inset
        spacing: 0
        Repeater {
            id: repeater
            model: root.items
            onItemAdded: thumb.update()
            Item {
                id: cell
                required property var modelData
                readonly property bool selected: modelData.value === root.value
                implicitWidth: content.implicitWidth + (root.compact ? 26 : 30)
                implicitHeight: root.height - root.inset * 2
                width: implicitWidth; height: implicitHeight
                Accessible.role: Accessible.RadioButton
                Accessible.name: modelData.text
                Accessible.checked: selected
                Accessible.onPressAction: root.activated(modelData.value)
                Row {
                    id: content
                    anchors.centerIn: parent
                    spacing: 6
                    Icon {
                        anchors.verticalCenter: parent.verticalCenter
                        name: cell.modelData.icon || ""
                        size: 15
                        tint: T.text
                        visible: name !== ""
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: cell.modelData.text
                        color: cell.selected || hover.hovered ? T.text : T.text2
                        font.pixelSize: root.compact ? T.callout : T.body
                        font.weight: cell.selected ? Font.DemiBold : Font.Normal
                    }
                }
                HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
                Tip { text: cell.modelData.tip || "" }
                TapHandler { onTapped: root.activated(cell.modelData.value) }
            }
        }
    }
}
