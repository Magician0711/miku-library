// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only
pragma Singleton
import QtQuick

// Where the window's floating chrome leaves room for page content.
QtObject {
    // Left edge of page content, past the sidebar.
    property real leading: 260
    // Right edge margin of page content.
    readonly property real trailing: 24
    // Set by the main window. Height decides the spacing, whatever the screen's shape or
    // scaling: short windows need their rows for content, tall ones can afford to breathe.
    property real windowHeight: 860
    readonly property real air: Math.max(0, Math.min(1, (windowHeight - 620) / 480))
    // The page title block, top to bottom.
    readonly property int headerTop: Math.round(14 + 10 * air)
    readonly property int titleLine: Math.round(30 * Theme.textScale)
    readonly property int headerGap: Math.round(3 + 3 * air)
    readonly property int subtitleLine: Math.round(17 * Theme.textScale)
    readonly property int headerBottom: Math.round(14 + 10 * air)
    // Height of the band the title and toolbar float in.
    readonly property int band: headerTop + titleLine + headerGap + subtitleLine + headerBottom
    // The same band on a page with a toolbar but no title.
    readonly property int toolBand: Math.round(60 + 12 * air)
    // Radius of the window's corners: the sidebar's radius plus its inset, so the two
    // curves run parallel. Square while maximized.
    property int corner: 24
    // How far the page content is still sliding after the sidebar changed its width.
    // Where the content starts once the sidebar is at rest; a pointer resting on a folded
    // sidebar does not count. Layouts that switch shape go by this, so that passing over
    // the sidebar does not rearrange the page.
    property real restingLeading: 260
    property real sidebarReveal: 1             // 0 folded, 1 open, in between while moving
    readonly property int railBelow: 1060      // narrower windows keep the sidebar folded
    readonly property int foldedLeading: 92
    readonly property int openLeading: 256
    // Width kept free for the caption buttons at the top right.
    property real caption: 190
    // How many pages have content under the window controls: one, or none.
    property int underCaption: 0
}
