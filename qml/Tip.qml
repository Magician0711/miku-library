// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only
import QtQuick

HoverHandler {
    id: handler
    property string text: ""
    onHoveredChanged: hovered && text !== "" ? TipHub.show(parent, text) : TipHub.hide(parent)
    onTextChanged: if (hovered) text !== "" ? TipHub.show(parent, text) : TipHub.hide(parent)
    Component.onDestruction: TipHub.hide(parent)
}
