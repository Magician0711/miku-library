// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Templates as Tpl

// Posters that always fill the width: the column count follows the space, and the cards
// share what is left, so no window size leaves an empty strip. Only the rows near the
// viewport exist. The grid spans the window, so it scrolls under the floating chrome.
Item {
    id: root
    required property var controller
    property string emptyIcon: "search"
    property string emptyActionText: ""
    signal emptyAction()
    property real topInset: Shell.band
    property Component header: null
    property alias contentY: grid.contentY
    property alias originY: grid.originY
    // How far the content has moved from its resting place.
    readonly property real scrolled: grid.contentY - grid.originY + grid.topMargin

    readonly property int gap: 20
    // The sidebar must not change how many columns there are: posters would jump to other
    // rows. Where it can open for good, count as if it were open, and only the size changes.
    readonly property real countedWidth: Math.max(1, width - Shell.trailing - (width >= Shell.railBelow ? Shell.openLeading : Shell.foldedLeading))
    // Large windows get larger posters as well as more of them.
    readonly property real minCard: Math.max(168, Math.min(230, countedWidth / 9))
    readonly property real available: Math.max(minCard, width - Shell.leading - Shell.trailing)
    readonly property int columns: Math.max(1, Math.floor((countedWidth + gap) / (minCard + gap)))
    function cardAt(room) { return (Math.max(minCard, room) - gap * (columns - 1)) / columns }
    // Whole pixels at both ends of the sidebar's travel, without a step where it stops:
    // what rounding takes off is shared out along the way.
    readonly property real foldedCard: cardAt(width - Shell.trailing - Shell.foldedLeading)
    readonly property real openCard: cardAt(width - Shell.trailing - Shell.openLeading)
    function along(atFolded, atOpen) { return atFolded + (atOpen - atFolded) * Shell.sidebarReveal }
    readonly property real cardWidth: cardAt(available) + along(Math.floor(foldedCard) - foldedCard, Math.floor(openCard) - openCard)
    function heightError(card) { const exact = Math.floor(card) * 1.414; return Math.round(exact) - exact }
    readonly property real posterHeight: cardWidth * 1.414 + along(heightError(foldedCard), heightError(openCard))

    // Covers are decoded for the largest size they take with this window, in steps, and
    // only once the window has stopped changing: following the card would reload every
    // poster on every frame of a sidebar motion or a window drag.
    property int decodeWidth: 0
    property real coverRadius: 12
    function settleCovers() {
        decodeWidth = Math.ceil(Math.max(1, foldedCard) / 40) * 40
        coverRadius = 12 * decodeWidth / Math.max(1, cardAt(countedWidth))
    }
    Timer { id: coverSettle; interval: 300; onTriggered: root.settleCovers() }
    onWidthChanged: {
        if (width > 0 && decodeWidth === 0)
            settleCovers()
        else
            coverSettle.restart()
    }

    GridView {
        id: grid
        anchors.fill: parent
        leftMargin: Shell.leading
        rightMargin: Shell.trailing - root.gap
        topMargin: root.topInset
        bottomMargin: 20
        cellWidth: root.cardWidth + root.gap
        cellHeight: root.posterHeight + 96
        model: root.controller.model
        header: root.header
        reuseItems: true
        cacheBuffer: 900
        interactive: false
        boundsBehavior: Flickable.StopAtBounds
        delegate: PosterCard {
            required property int index
            // Tabbing to a poster that is off screen brings it into view.
            onActiveFocusChanged: if (activeFocus) grid.positionViewAtIndex(index, GridView.Contain)
            cardWidth: root.cardWidth
            posterHeight: root.posterHeight
            decodeWidth: root.decodeWidth
            coverRadius: root.coverRadius
            onClicked: id => root.controller.open(id)
        }
        Tpl.ScrollBar.vertical: NxScrollBar { windowEdge: true }
        SmoothWheel { id: wheel; view: grid }
        // A header that grows pushes the content upwards, out of view: stay with the top,
        // at once, or the frame being drawn shows the heading pushed up.
        function settle() { if (wheel.pinned) contentY = originY - topMargin }
        onOriginYChanged: { settle(); Qt.callLater(settle) }
        onTopMarginChanged: Qt.callLater(settle)
        onCountChanged: Qt.callLater(settle)
        // The view does not follow its own margin.
        onLeftMarginChanged: contentX = -leftMargin
    }
    EmptyState {
        x: Shell.leading
        y: root.topInset
        width: root.available
        height: root.height - root.topInset - 40
        iconName: root.emptyIcon
        title: root.controller.emptyTitle
        description: root.controller.emptyText
        shown: root.controller.emptyVisible
        actionText: root.emptyActionText
        onAction: root.emptyAction()
    }
}
