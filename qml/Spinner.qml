// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only
import QtQuick

// A turning arc. The rotation runs on the render thread, so a busy UI thread does not
// stop it.
Icon {
    id: root
    name: "spinner"
    tint: T.accentText
    RotationAnimator on rotation {
        running: root.visible && Theme.animated
        from: 0; to: 360
        duration: 900
        loops: Animation.Infinite
    }
}
