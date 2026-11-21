// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Templates as Tpl
import QtQuick.Window

Window {
    id: window
    width: 1320; height: 860
    minimumWidth: 900; minimumHeight: 620
    title: "Miku Library"
    color: "transparent"
    // macOS keeps its title bar, transparent, for the system's own window buttons.
    flags: (Chrome.systemButtons ? Qt.WindowTitleHint | Qt.NoTitleBarBackgroundHint : Qt.FramelessWindowHint)
           | Qt.Window | Qt.WindowSystemMenuHint | Qt.WindowMinMaxButtonsHint | Qt.WindowCloseButtonHint

    readonly property var pageNames: [qsTr("本季放送"), qsTr("搜索番剧"), qsTr("我的追番"), qsTr("下载任务"), qsTr("资源站点"), qsTr("偏好设置"), qsTr("番剧详情"), qsTr("正在播放")]
    readonly property var pageIcons: ["calendar", "search", "library", "download", "sites", "settings", "search", "play"]
    readonly property int downloadsPage: 3
    readonly property int sitesPage: 4
    readonly property int settingsPage: 5
    readonly property int detailPage: 6
    readonly property int playerPage: 7
    readonly property bool immersive: App.page === playerPage && visibility === Window.FullScreen
    // Set by the player page: three seconds without the pointer moving hides the navigation
    // over the picture as well; the player reads pointerOnNav to come back.
    property bool playerResting: false
    readonly property bool pointerOnNav: barHover.hovered

    Binding { target: Shell; property: "windowHeight"; value: window.height }
    // Corners: square when maximised; macOS rounds the window itself; with the desktop
    // showing through, the window takes the system's rounded corners, since the material
    // fills its rectangle; otherwise the interface draws its own at 24.
    Binding {
        target: Shell; property: "corner"
        value: window.visibility === 4 || Chrome.systemButtons ? 0 : Chrome.translucent ? 8 : 24
    }
    Binding { target: Shell; property: "buttonInset"; value: Chrome.buttonInset }
    // Pages run on beneath the navigation bar and stop this far above the window's edge.
    Binding { target: Shell; property: "bottomInset"; value: window.immersive ? 0 : navBar.height + 2 * navBar.margin }

    Component.onCompleted: {
        Chrome.setDark(Theme.dark)
        // The desktop shows through, unless transparency or contrast settings say otherwise.
        Chrome.setBackdrop(Theme.transparency && !Theme.highContrast)
        Chrome.attach(window)
        visible = true
    }
    Connections {
        target: Theme
        function onDarkChanged() { Chrome.setDark(Theme.dark) }
    }
    onClosing: close => {
        close.accepted = false
        App.requestClose()
    }

    Shortcut { sequence: "Ctrl+K"; onActivated: App.selectPage(1) }
    Shortcut { sequence: "Ctrl+,"; onActivated: App.selectPage(window.settingsPage) }
    Shortcut {
        sequence: "Esc"
        enabled: App.page === window.sitesPage && Object.keys(App.dialogs.current).length === 0
        onActivated: App.selectPage(window.settingsPage)
    }
    Shortcut {
        sequence: "Esc"
        enabled: App.page === window.detailPage && Object.keys(App.dialogs.current).length === 0
                 && Object.keys(App.detail.groupDialog).length === 0
        onActivated: App.closeDetail()
    }

    // The window's background: the page colour, or a tint of it over the system's blur of
    // the desktop.
    Rectangle {
        objectName: "windowBackground"
        parent: window.contentItem
        anchors.fill: parent
        radius: Shell.corner
        color: Chrome.translucent ? T.bgTint : T.bg
    }

    // Moves the window from any empty part of the top band.
    Item {
        width: parent.width; height: Shell.band
        TapHandler {
            gesturePolicy: TapHandler.DragThreshold
            onDoubleTapped: window.visibility === 4 ? window.showNormal() : window.showMaximized()
        }
        DragHandler {
            target: null
            onActiveChanged: if (active) window.startSystemMove()
        }
    }

    // Content layer: pages fill the window, the floating layer sits over them.
    Item {
        id: content
        width: parent.width
        height: parent.height
        component Page: Loader {
            required property int page
            readonly property bool current: App.page === page
            anchors.fill: parent
            active: false
            opacity: current ? 1 : 0
            visible: opacity > 0
            enabled: current
            onCurrentChanged: if (current) active = true
            Component.onCompleted: if (current) active = true
            Behavior on opacity { NumberAnimation { duration: T.normal; easing.type: Easing.OutCubic } }
        }
        Page { page: 0; source: "HomePage.qml" }
        Page { page: 1; source: "SearchPage.qml" }
        Page { page: 2; source: "LibraryPage.qml" }
        Page { page: 3; source: "DownloadsPage.qml" }
        Page { page: 4; source: "SitesPage.qml" }
        Page { page: 5; source: "SettingsPage.qml" }
        Page { page: 6; source: "DetailPage.qml" }
        Page { page: 7; source: "PlayerPage.qml" }
    }

    // Navigation: a Liquid Glass bar floating over the bottom of every page. The pages run
    // on beneath it (Shell.bottomInset), so the glass always has content to bend. At rest
    // it shows large icons; pointing at it or moving the keyboard into it opens the names,
    // and the icons shrink to make room. A window too narrow for the names keeps the icons
    // and shows each name in a tip instead.
    Glass {
        id: navBar
        objectName: "navBar"
        visible: !window.immersive
        // Still there while hidden, so moving the pointer onto it brings it back.
        opacity: window.playerResting && !focusInside ? 0 : 1
        Behavior on opacity { NumberAnimation { duration: T.normal; easing.type: Easing.OutCubic } }
        backdrop: content
        liquid: false
        shadow: false
        readonly property int margin: 14
        readonly property int pad: 6
        // Room for every name, measured without regard to how the bar is drawn now.
        readonly property real fullWidth: {
            // The Repeater is a child too, but takes no place in the row.
            let total = 0, count = 0
            for (const child of navRow.children) {
                if (child.fullWidth === undefined)
                    continue
                total += child.fullWidth
                ++count
            }
            return total + navRow.spacing * Math.max(0, count - 1) + 2 * pad
        }
        readonly property bool compact: fullWidth > window.width - 2 * 24
        readonly property real restingButton: 48
        readonly property bool focusInside: window.activeFocusItem !== null && window.activeFocusItem.parent === navRow
        // Names open at once and close a moment after the pointer leaves, so passing over
        // the edge does not make the bar pulse.
        property bool expanded: false
        readonly property bool wantsNames: !compact && (barHover.hovered || focusInside)
        onWantsNamesChanged: wantsNames ? (closeNames.stop(), expanded = true) : closeNames.restart()
        Timer { id: closeNames; interval: 280; onTriggered: navBar.expanded = navBar.wantsNames }
        // 0 at rest, 1 with the names open; everything in the bar follows this one value.
        property real reveal: expanded ? 1 : 0
        Behavior on reveal { NumberAnimation { duration: T.normal; easing.type: Easing.OutCubic } }
        HoverHandler { id: barHover }
        // The pages run on under the bar, and a press on the bar must not reach them: a
        // click on a button used to open the poster beneath it as well. The buttons keep
        // their own presses; this catches the rest of the bar.
        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.AllButtons
            onPressed: mouse => mouse.accepted = true
        }
        width: navRow.width + 2 * pad
        height: 52
        // At rest the bar is centred. As the names open it grows around its middle item
        // (我的追番), which stays where it was, rather than around the window's centre;
        // it only gives way where the window's edge would cut it off.
        readonly property Item middle: {
            const items = []
            for (const child of navRow.children)
                if (child.fullWidth !== undefined)
                    items.push(child)
            return items.length ? items[Math.floor(items.length / 2)] : null
        }
        // Where the middle item's centre lies in the bar when every button is at rest.
        readonly property real restingMiddle: {
            let offset = pad
            for (const child of navRow.children) {
                if (child.fullWidth === undefined)
                    continue
                const resting = child.page !== undefined ? restingButton : child.fullWidth
                if (child === middle)
                    return offset + resting / 2
                offset += resting + navRow.spacing
            }
            return 0
        }
        readonly property real restingWidth: {
            let total = 0, count = 0
            for (const child of navRow.children) {
                if (child.fullWidth === undefined)
                    continue
                total += child.page !== undefined ? restingButton : child.fullWidth
                ++count
            }
            return total + navRow.spacing * Math.max(0, count - 1) + 2 * pad
        }
        x: {
            const anchor = (parent.width - restingWidth) / 2 + restingMiddle
            const place = middle ? anchor - (navRow.x + middle.x + middle.width / 2) : (parent.width - width) / 2
            return Math.round(Math.max(24, Math.min(parent.width - width - 24, place)))
        }
        anchors.bottom: parent.bottom
        anchors.bottomMargin: margin

        // The selection: a lens that slides and stretches from one item to the next.
        Rectangle {
            id: navThumb
            objectName: "navThumb"
            property Item target: null
            // It glides between the item it left and the one it is going to, both measured
            // live, so it stays on them while the bar opens or closes underneath.
            property Item from: null
            property real glide: 1
            function moveTo(item) {
                if (item === target)
                    return
                from = target
                target = item
                glideAnimation.restart()
            }
            NumberAnimation {
                id: glideAnimation
                target: navThumb; property: "glide"
                from: 0; to: 1
                duration: navThumb.visible && navThumb.from ? T.glide : 0
                easing.type: Easing.BezierSpline; easing.bezierCurve: T.glideCurve
            }
            readonly property real startX: from ? from.x : target ? target.x : 0
            readonly property real startWidth: from ? from.width : target ? target.width : 0
            x: target ? navRow.x + startX + (target.x - startX) * glide : 0
            y: (navBar.height - height) / 2
            width: target ? startWidth + (target.width - startWidth) * glide : 0
            height: 40
            radius: height / 2
            color: T.dark ? Qt.rgba(1, 1, 1, 0.13) : Qt.rgba(1, 1, 1, 0.72)
            border.width: 1
            border.color: T.dark ? Qt.rgba(1, 1, 1, 0.1) : Qt.rgba(1, 1, 1, 0.9)
            visible: target !== null && App.page !== window.detailPage

        }
        component NavButton: Item {
            id: nav
            required property int page
            property string badge: ""
            readonly property string name: window.pageNames[page]
            readonly property real fullWidth: 14 + 18 + 8 + label.implicitWidth + 16
            width: navBar.restingButton + (fullWidth - navBar.restingButton) * navBar.reveal
            height: 40
            y: (navRow.height - height) / 2
            // The site list is a page of the preferences.
            readonly property bool selected: App.page === page || (page === window.settingsPage && App.page === window.sitesPage)
            onSelectedChanged: if (selected) navThumb.moveTo(nav)
            Component.onCompleted: if (selected) navThumb.moveTo(nav)
            Accessible.role: Accessible.Button
            Accessible.name: name
            Accessible.onPressAction: App.selectPage(page)
            activeFocusOnTab: true
            readonly property bool visualFocus: activeFocus
            Keys.onReturnPressed: App.selectPage(page)
            Keys.onEnterPressed: App.selectPage(page)
            Keys.onSpacePressed: App.selectPage(page)
            Rectangle {
                anchors.fill: parent
                radius: height / 2
                color: hover.hovered && !nav.selected ? T.fill : "transparent"
                Behavior on color { ColorAnimation { duration: T.fast } }
            }
            FocusRing { target: nav; corner: nav.height / 2 }
            // Drawn at the resting size and scaled down as the names open, so it is
            // rasterised once. At rest it is centred; open, it sits where the name begins.
            Icon {
                id: glyph
                x: (navBar.restingButton - size) / 2 + (14 - (size - 18) / 2 - (navBar.restingButton - size) / 2) * navBar.reveal
                anchors.verticalCenter: parent.verticalCenter
                size: 22
                scale: 1 - (1 - 18 / size) * navBar.reveal
                name: window.pageIcons[nav.page]
                tint: nav.selected ? T.accentText : T.text2
            }
            Text {
                id: label
                x: 14 + 18 + 8
                anchors.verticalCenter: parent.verticalCenter
                // Fades in once there is room for it.
                opacity: Math.max(0, (navBar.reveal - 0.4) / 0.6)
                visible: opacity > 0
                text: nav.name
                color: nav.selected ? T.accentText : T.text
                font.pixelSize: T.body
                font.weight: nav.selected ? Font.DemiBold : Font.Normal
            }
            Rectangle {
                x: glyph.x + glyph.width / 2 + glyph.width * glyph.scale / 2 - 6
                y: 3
                visible: nav.badge !== ""
                width: Math.max(16, count.implicitWidth + 9); height: 16; radius: 8
                color: T.accent
                Text { id: count; anchors.centerIn: parent; text: nav.badge; color: T.accentInk; font.pixelSize: T.footnote; font.weight: Font.Bold }
            }
            HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
            Tip { text: navBar.compact ? nav.name : "" }
            // Takes the press for itself, so nothing beneath the bar sees it.
            TapHandler { gesturePolicy: TapHandler.ReleaseWithinBounds; onTapped: App.selectPage(nav.page) }
        }
        component Separator: Rectangle {
            readonly property real fullWidth: 1
            width: 1; height: 22
            y: (navRow.height - height) / 2
            color: T.separator
        }
        Row {
            id: navRow
            x: navBar.pad
            height: parent.height
            spacing: 2
            // The logo leads back to the home page.
            Item {
                id: home
                objectName: "homeButton"
                readonly property real fullWidth: 44
                width: 44; height: 40
                y: (navRow.height - height) / 2
                Accessible.role: Accessible.Button
                Accessible.name: "Miku Library"
                Accessible.description: window.pageNames[0]
                Accessible.onPressAction: App.selectPage(0)
                activeFocusOnTab: true
                readonly property bool visualFocus: activeFocus
                Keys.onReturnPressed: App.selectPage(0)
                Keys.onEnterPressed: App.selectPage(0)
                Keys.onSpacePressed: App.selectPage(0)
                Rectangle {
                    anchors.fill: parent
                    radius: height / 2
                    color: homeHover.hovered ? T.fill : "transparent"
                    Behavior on color { ColorAnimation { duration: T.fast } }
                }
                FocusRing { target: home; corner: home.height / 2 }
                HoverHandler { id: homeHover; cursorShape: Qt.PointingHandCursor }
                TapHandler { gesturePolicy: TapHandler.ReleaseWithinBounds; onTapped: App.selectPage(0) }
                Image {
                    objectName: "brandLogo"
                    anchors.centerIn: parent
                    width: 28; height: 28
                    source: "../assets/logo.png"
                    sourceSize: Qt.size(Math.ceil(56 * Screen.devicePixelRatio), Math.ceil(56 * Screen.devicePixelRatio))
                    fillMode: Image.PreserveAspectFit
                    mipmap: true
                    Accessible.ignored: true
                }
            }
            Separator {}
            Repeater {
                model: [0, 1, 2, 3, window.playerPage]
                NavButton {
                    required property int modelData
                    page: modelData
                    badge: page === window.downloadsPage && App.activeDownloads > 0 ? App.activeDownloads : ""
                }
            }
            Separator {}
            NavButton { page: window.settingsPage }
        }
    }

    // Thin glyphs need something behind them once posters pass underneath: a small piece
    // of glass gathers the window controls, as a scroll edge would.
    Glass {
        objectName: "captionBacking"
        backdrop: content
        readonly property real centre: caption.topPadding + (caption.height - caption.topPadding) / 2
        x: caption.x - 6
        y: centre - height / 2
        width: window.width - x - (Chrome.systemButtons ? 6 : 7)
        height: 36
        opacity: Shell.underCaption > 0 && !window.immersive ? 1 : 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: T.fast; easing.type: Easing.OutCubic } }
    }

    // Window controls and the appearance menu, top right of every page.
    Row {
        id: caption
        visible: !window.immersive
        anchors.right: parent.right
        // Without the drawn window buttons (macOS), the menu keeps clear of the corner and
        // lines up with the traffic lights on the other side.
        anchors.rightMargin: Chrome.systemButtons ? 12 : 0
        topPadding: Chrome.systemButtons ? 6 : 0
        spacing: 6
        onWidthChanged: Shell.caption = width + 18
        Rectangle {
            // Appears only when GitHub has a newer release; opens its page.
            anchors.verticalCenter: parent.verticalCenter
            visible: App.newVersion !== ""
            width: versionText.implicitWidth + 22; height: 26; radius: 13
            color: versionHover.hovered ? T.accentSoft2 : T.accentSoft
            Text {
                id: versionText
                anchors.centerIn: parent
                text: App.newVersion
                color: T.accentText
                font.pixelSize: T.callout
                font.weight: Font.DemiBold
            }
            HoverHandler { id: versionHover; cursorShape: Qt.PointingHandCursor }
            Tip { text: qsTr("打开下载页面") }
            TapHandler { onTapped: App.openNewVersion() }
        }
        IconButton {
            id: themeButton
            anchors.verticalCenter: parent.verticalCenter
            side: 30
            iconName: Theme.mode === "dark" ? "moon" : Theme.mode === "light" ? "sun" : "monitor"
            tip: qsTr("外观：%1").arg(Theme.mode === "dark" ? qsTr("深色") : Theme.mode === "light" ? qsTr("浅色") : qsTr("跟随系统"))
            focusPolicy: Qt.TabFocus
            onClicked: themeMenu.popup(themeButton, themeButton.width - 220, themeButton.height + 4)
            NxMenu {
                id: themeMenu
                NxMenuItem { text: qsTr("跟随系统"); iconName: "monitor"; checkable: true; checked: Theme.mode === "system"; onTriggered: App.settings.setAppearance("system") }
                NxMenuItem { text: qsTr("浅色"); iconName: "sun"; checkable: true; checked: Theme.mode === "light"; onTriggered: App.settings.setAppearance("light") }
                NxMenuItem { text: qsTr("深色"); iconName: "moon"; checkable: true; checked: Theme.mode === "dark"; onTriggered: App.settings.setAppearance("dark") }
            }
        }
        WindowButtons { window: window; visible: !Chrome.systemButtons }
    }

    UpdatePanel { anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 20; anchors.bottomMargin: 20 + Shell.bottomInset }
    Toast { id: toast; anchors.centerIn: parent }
    Connections {
        target: App.dialogs
        function onToastRequested(text, icon) { toast.show(text, icon) }
    }

    DownloaderPopup { id: downloaderPopup }
    DialogHost {}
    GroupDownloadDialog {}
    function chooseDownloader(anchor) { downloaderPopup.showAt(anchor) }

    // A hairline around the window, in place of the border Windows draws at its own radius.
    Rectangle {
        parent: window.contentItem
        anchors.fill: parent
        z: 2500000
        visible: Shell.corner > 0
        radius: Shell.corner
        color: "transparent"
        border.width: 1
        border.color: T.glassEdge
    }
    TipLayer { parent: window.contentItem }
    ThemeFade { parent: window.contentItem }
    ResizeEdges { parent: window.contentItem; window: window }
}
