// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only
import QtQuick

// A page: its content fills the window, with the title and toolbar in a band at the top.
// Where the content scrolls under that band, they leave with it, so nothing has to cover
// the content to keep the two apart. Pages with a fixed row clip their own lists instead.
Item {
    id: page
    property string title: ""
    property string subtitle: ""
    property string subtitleTip: ""
    // Height of what stays fixed under the toolbar, such as a search row.
    property real fixedHeight: 0
    property real band: Shell.band
    // How far the content has scrolled, for pages whose content runs under the band.
    property real scrolled: 0
    readonly property real lift: Math.max(0, scrolled)
    // Set to show a back button before the title; its tip says where it leads.
    property string backTip: ""
    signal back()
    default property alias content: body.data
    property alias toolbar: tools.data
    // Stays in place under the toolbar while the content scrolls.
    property alias fixed: fixedLayer.data
    // For anything else that stays above the content.
    readonly property Item overlay: fixedLayer

    readonly property real leading: Shell.leading
    readonly property real restingWidth: width - Shell.restingLeading - Shell.trailing
    readonly property real trailing: Shell.trailing
    readonly property real contentTop: band + fixedHeight
    // The window's own controls stay; tell them when content reaches them. A count, as
    // pages hand over in no particular order.
    readonly property bool underCaption: enabled && lift > contentTop - 48
    onUnderCaptionChanged: Shell.underCaption += underCaption ? 1 : -1
    Component.onDestruction: if (underCaption) Shell.underCaption -= 1

    Item { id: body; anchors.fill: parent }

    Item { id: fixedLayer; anchors.fill: parent }
    IconButton {
        id: backButton
        x: page.leading - 9
        y: Shell.headerTop + Shell.titleLine / 2 - side / 2 - page.lift
        visible: page.backTip !== ""
        side: 34
        iconName: "chevron-left"
        tip: page.backTip
        onClicked: page.back()
    }
    Column {
        x: page.leading + (backButton.visible ? 28 : 0)
        y: Shell.headerTop - page.lift
        width: parent.width - x - tools.width - Shell.caption - 16
        spacing: Shell.headerGap
        Text {
            width: parent.width
            text: page.title
            color: T.text
            font.pixelSize: T.title1
            font.weight: Font.Bold
            lineHeightMode: Text.FixedHeight
            lineHeight: Shell.titleLine
            elide: Text.ElideRight
        }
        Text {
            width: parent.width
            text: page.subtitle
            visible: text !== ""
            color: T.text3
            font.pixelSize: T.callout
            lineHeightMode: Text.FixedHeight
            lineHeight: Shell.subtitleLine
            elide: Text.ElideRight
            Tip { text: page.subtitleTip }
        }
    }
    Row {
        id: tools
        anchors.right: parent.right
        anchors.rightMargin: Shell.caption
        // Level with the window controls.
        y: 8 - page.lift
        spacing: 8
    }
}
