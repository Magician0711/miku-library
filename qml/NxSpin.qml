// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Templates as Tpl

Tpl.SpinBox {
    id: c
    property string prefix: ""
    property string suffix: ""
    property string zeroText: ""    // shown instead of 0
    signal edited(int value)

    editable: true
    implicitWidth: 180
    implicitHeight: 32
    leftPadding: 14; rightPadding: 62
    hoverEnabled: true
    focusPolicy: Qt.TabFocus
    inputMethodHints: Qt.ImhDigitsOnly
    textFromValue: (value, locale) => value === 0 && zeroText !== "" ? zeroText : prefix + value + suffix
    valueFromText: (text, locale) => {
        if (text === zeroText) return 0
        const digits = text.replace(/[^0-9]/g, "")
        return digits === "" ? from : parseInt(digits)
    }
    onValueModified: edited(value)
    background: Rectangle {
        radius: height / 2
        color: T.fill
        border.width: c.activeFocus ? 2 : 0
        border.color: T.accent
    }
    contentItem: TextInput {
        text: c.displayText
        color: c.enabled ? T.text : T.disabledText
        font.pixelSize: T.body
        verticalAlignment: TextInput.AlignVCenter
        selectionColor: T.accentSoft2
        selectedTextColor: T.text
        selectByMouse: true
        readOnly: !c.editable
        inputMethodHints: c.inputMethodHints
        clip: true
    }
    component Step: Rectangle {
        id: step
        required property bool isDown
        required property bool isHovered
        required property string glyph
        implicitWidth: 26
        implicitHeight: 26
        radius: 13
        color: isDown ? T.fillPressed : isHovered && c.enabled ? T.fillHover : "transparent"
        Icon { anchors.centerIn: parent; size: 13; name: step.glyph; tint: c.enabled ? T.text : T.disabledText }
    }
    down.indicator: Step {
        x: c.width - 58; y: 3
        isDown: c.down.pressed; isHovered: c.down.hovered; glyph: "minus"
    }
    up.indicator: Step {
        x: c.width - 30; y: 3
        isDown: c.up.pressed; isHovered: c.up.hovered; glyph: "plus"
    }
}
