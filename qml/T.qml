// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only
pragma Singleton
import QtQuick

// Design tokens. Roles, not looks: a colour keeps its meaning everywhere, and every text
// colour here holds 4.5:1 or better against the surfaces it is used on.
QtObject {
    readonly property bool dark: Theme.dark
    readonly property bool strong: Theme.highContrast

    // Surfaces
    readonly property color bg: dark ? "#161618" : "#f5f5f7"
    readonly property color surface: dark ? "#242427" : "#ffffff"
    readonly property color surface2: dark ? "#2d2d31" : "#f2f2f4"
    readonly property color surface3: dark ? "#39393e" : "#e8e8ec"
    // A control's resting fill, over any surface.
    readonly property color fill: dark ? Qt.rgba(1, 1, 1, 0.09) : Qt.rgba(0.24, 0.24, 0.26, 0.08)
    readonly property color fillHover: dark ? Qt.rgba(1, 1, 1, 0.14) : Qt.rgba(0.24, 0.24, 0.26, 0.13)
    readonly property color fillPressed: dark ? Qt.rgba(1, 1, 1, 0.2) : Qt.rgba(0.24, 0.24, 0.26, 0.19)
    readonly property color separator: strong ? borderStrong : dark ? Qt.rgba(1, 1, 1, 0.1) : Qt.rgba(0.24, 0.24, 0.26, 0.13)
    readonly property color border: strong ? borderStrong : dark ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(0.24, 0.24, 0.26, 0.16)
    readonly property color borderStrong: dark ? Qt.rgba(1, 1, 1, 0.26) : Qt.rgba(0.24, 0.24, 0.26, 0.34)

    // The floating layer: sidebar glass, and its solid fallback.
    readonly property color glass: dark ? Qt.rgba(0.16, 0.16, 0.18, 0.62) : Qt.rgba(0.97, 0.97, 0.98, 0.66)
    readonly property color glassSolid: dark ? "#232326" : "#ececef"
    readonly property color glassEdge: dark ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(1, 1, 1, 0.75)

    // Text
    readonly property color text: dark ? "#f5f5f7" : "#1d1d1f"
    readonly property color text2: strong ? text : dark ? "#c7c7cc" : "#56565c"
    readonly property color text3: strong ? text : dark ? "#a0a0a7" : "#6a6a70"
    readonly property color disabledText: dark ? "#6c6c73" : "#a9a9b0"

    // Brand colour: primary actions, selection and status only.
    readonly property color accent: "#39c5bb"
    readonly property color accentHover: dark ? "#4dd2c8" : "#31b6ac"
    readonly property color accentPressed: dark ? "#31b2a8" : "#2aa69d"
    readonly property color accentInk: "#06312e"
    readonly property color accentText: dark ? "#5fd8ce" : "#0a6f66"
    readonly property color accentSoft: dark ? "#15403c" : "#d9f3f0"
    readonly property color accentSoft2: dark ? "#1b504b" : "#c3ebe6"

    // Status
    readonly property color ok: dark ? "#4cd28f" : "#1a7f5a"
    readonly property color warn: dark ? "#ffb340" : "#96600a"
    readonly property color warnSoft: dark ? "#3d3019" : "#fbf1dc"
    readonly property color err: dark ? "#ff6961" : "#c9281e"
    readonly property color errSoft: dark ? "#40211f" : "#fdeae8"

    // Quality tags
    readonly property color probedBg: dark ? "#193630" : "#dff3ec"
    readonly property color probedFg: dark ? "#6fd3ac" : "#1a6f58"
    readonly property color guessedBg: dark ? "#3a311b" : "#f6f0e1"
    readonly property color guessedFg: dark ? "#e3c06f" : "#7a5d17"
    readonly property color unknownBg: dark ? "#303035" : "#eeeef1"
    readonly property color unknownFg: dark ? "#a6a6ae" : "#62626a"
    readonly property color hdrBg: dark ? "#2f2840" : "#efeaf6"
    readonly property color hdrFg: dark ? "#bea8e3" : "#654f83"
    readonly property color subBg: dark ? "#1f2d3b" : "#e7eef6"
    readonly property color subFg: dark ? "#8db6de" : "#3a5d80"

    readonly property color shadow: dark ? Qt.rgba(0, 0, 0, 0.42) : Qt.rgba(0, 0, 0, 0.1)

    // Text styles, in logical pixels.
    readonly property int largeTitle: Math.round(28 * Theme.textScale)
    readonly property int title1: Math.round(22 * Theme.textScale)
    readonly property int title2: Math.round(17 * Theme.textScale)
    readonly property int title3: Math.round(15 * Theme.textScale)
    readonly property int body: Math.round(13 * Theme.textScale)
    readonly property int callout: Math.round(12 * Theme.textScale)
    readonly property int footnote: Math.round(11 * Theme.textScale)

    readonly property int fast: Theme.animated ? 140 : 0     // hover, popups
    readonly property int normal: Theme.animated ? 220 : 0   // selection, expand / collapse, page changes, theme change
    readonly property int slow: Theme.animated ? 320 : 0     // notices leaving by themselves
    readonly property int glide: Theme.animated ? 380 : 0    // large surfaces moving, such as the sidebar
    readonly property var glideCurve: [0.32, 0.72, 0, 1, 1, 1]   // leaves at once, settles slowly

    function tone(name) {
        switch (name) {
        case "active": return accentText
        case "done": return ok
        case "error": return err
        case "wait": return warn
        }
        return text3
    }
    function dot(state) {
        switch (state) {
        case "ok": return ok
        case "warn": return warn
        case "err": return err
        case "busy": return accentText   // the brand colour itself is too light on the light page
        }
        return text3
    }
}
