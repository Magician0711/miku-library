// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only
import QtQuick

// Linux: a frameless window has no edges to resize by, so the interface lays a thin band
// along each side and corner and hands the drag to the window manager. Windows and macOS
// resize through the system frame (see WindowChrome).
Item {
    id: root
    required property var window
    readonly property int band: 6
    readonly property int corner: 14
    anchors.fill: parent
    z: 3000000
    visible: Chrome.resizeEdges && window.visibility === 2   // Window.Windowed

    component Grip: Item {
        id: grip
        required property int edges
        required property int shape
        HoverHandler { cursorShape: grip.shape }
        DragHandler {
            target: null
            grabPermissions: PointerHandler.CanTakeOverFromAnything
            onActiveChanged: if (active) root.window.startSystemResize(grip.edges)
        }
    }

    Grip { objectName: "resizeLeft"; edges: Qt.LeftEdge; shape: Qt.SizeHorCursor; x: 0; y: root.corner; width: root.band; height: parent.height - 2 * root.corner }
    Grip { edges: Qt.RightEdge; shape: Qt.SizeHorCursor; x: parent.width - root.band; y: root.corner; width: root.band; height: parent.height - 2 * root.corner }
    Grip { edges: Qt.TopEdge; shape: Qt.SizeVerCursor; x: root.corner; y: 0; width: parent.width - 2 * root.corner; height: root.band }
    Grip { edges: Qt.BottomEdge; shape: Qt.SizeVerCursor; x: root.corner; y: parent.height - root.band; width: parent.width - 2 * root.corner; height: root.band }
    Grip { edges: Qt.TopEdge | Qt.LeftEdge; shape: Qt.SizeFDiagCursor; width: root.corner; height: root.corner }
    Grip { edges: Qt.TopEdge | Qt.RightEdge; shape: Qt.SizeBDiagCursor; x: parent.width - root.corner; width: root.corner; height: root.corner }
    Grip { edges: Qt.BottomEdge | Qt.LeftEdge; shape: Qt.SizeBDiagCursor; y: parent.height - root.corner; width: root.corner; height: root.corner }
    Grip { objectName: "resizeCorner"; edges: Qt.BottomEdge | Qt.RightEdge; shape: Qt.SizeFDiagCursor; x: parent.width - root.corner; y: parent.height - root.corner; width: root.corner; height: root.corner }
}
