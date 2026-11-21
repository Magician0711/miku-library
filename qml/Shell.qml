// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only
pragma Singleton
import QtQuick

// Where the window's floating chrome leaves room for page content.
QtObject {
    // Left edge of page content. Navigation floats at the bottom, so nothing sits beside it.
    readonly property real leading: 24
    // Right edge margin of page content.
    readonly property real trailing: 24
    // Set by the main window. Height decides the spacing, whatever the screen's shape or
    // scaling: short windows need their rows for content, tall ones can afford to breathe.
    property real windowHeight: 860
    readonly property real air: Math.max(0, Math.min(1, (windowHeight - 620) / 480))
    // The page title block, top to bottom.
    // Room at the very top for the system's window buttons (macOS), set by the main window.
    property int buttonInset: 0
    readonly property int headerTop: Math.round(14 + 10 * air) + buttonInset
    readonly property int titleLine: Math.round(30 * Theme.textScale)
    readonly property int headerGap: Math.round(3 + 3 * air)
    readonly property int subtitleLine: Math.round(17 * Theme.textScale)
    readonly property int headerBottom: Math.round(14 + 10 * air)
    // Height of the band the title and toolbar float in.
    readonly property int band: headerTop + titleLine + headerGap + subtitleLine + headerBottom
    // The same band on a page with a toolbar but no title.
    readonly property int toolBand: Math.round(60 + 12 * air)
    // Radius of the window's corners, set by the main window: its own 24, or the system's
    // when the desktop shows through (WindowChrome.translucent). Square while maximized.
    property int corner: 24
    // Height the floating navigation bar takes at the bottom, with its margins. Lists
    // scroll on beneath it and end this far above the window's edge.
    property real bottomInset: 0
    // Width kept free for the caption buttons at the top right.
    property real caption: 190
    // How many pages have content under the window controls: one, or none.
    property int underCaption: 0
}
