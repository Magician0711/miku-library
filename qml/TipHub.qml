// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only
pragma Singleton
import QtQuick

// The one tooltip of a window: Tip reports what is under the pointer, TipLayer draws it.
QtObject {
    property Item owner: null
    property string text: ""
    function show(item, value) { owner = item; text = value }
    function hide(item) { if (owner === item) { owner = null; text = "" } }
}
