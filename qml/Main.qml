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
    flags: Qt.Window | Qt.FramelessWindowHint | Qt.WindowSystemMenuHint | Qt.WindowMinMaxButtonsHint | Qt.WindowCloseButtonHint

    readonly property var pageNames: ["本季放送", "搜索番剧", "我的追番", "下载任务", "资源站点", "偏好设置", "番剧详情", "正在播放"]
    readonly property var pageIcons: ["calendar", "search", "library", "download", "sites", "settings", "search", "play"]
    readonly property int downloadsPage: 3
    readonly property int sitesPage: 4
    readonly property int settingsPage: 5
    readonly property int detailPage: 6
    readonly property int playerPage: 7
    readonly property bool immersive: App.page === playerPage && visibility === Window.FullScreen

    // The sidebar steps aside for the content on narrow windows and on a detail page.
    property bool railChosen: false
    property bool detailFold: false
    readonly property bool rail: railChosen || detailFold || width < Shell.railBelow
    function toggleSidebar() {
        if (rail) { detailFold = false; railChosen = false }
        else railChosen = true
    }
    // Resting the pointer on a folded sidebar opens it and pushes the content aside, so
    // nothing is covered; leaving folds it again.
    property bool peekWanted: false
    readonly property bool peek: rail && peekWanted
    Binding { target: Shell; property: "windowHeight"; value: window.height }
    readonly property int sidebarWidth: rail ? 60 : 224
    readonly property int sidebarInset: 8
    // 0 folded, 1 open. Everything in the sidebar follows this one value, so it all moves
    // as a single piece.
    property real sidebarReveal: rail && !peek ? 0 : 1
    Behavior on sidebarReveal { NumberAnimation { duration: T.glide; easing.type: Easing.BezierSpline; easing.bezierCurve: T.glideCurve } }
    readonly property real sidebarShown: 60 + 164 * sidebarReveal
    // Text comes in once there is room for it.
    readonly property real labelAlpha: Math.max(0, (sidebarReveal - 0.45) / 0.55)
    property bool tracingSidebar: false
    onSidebarRevealChanged: {
        const moving = sidebarReveal > 0 && sidebarReveal < 1
        if (moving === tracingSidebar) return
        tracingSidebar = moving
        if (moving) Chrome.beginTrace("sidebar")
        else Chrome.endTrace()
    }
    // The pages follow the sidebar's edge on every frame, so they resize with it instead
    // of jumping to a new layout. Covers keep one decoded size meanwhile (see PosterGrid).
    readonly property real contentLeft: sidebarShown + sidebarInset + 24
    Binding { target: Shell; property: "sidebarReveal"; value: window.sidebarReveal }
    Binding { target: Shell; property: "restingLeading"; value: window.sidebarWidth + window.sidebarInset + 24 }
    Binding { target: Shell; property: "corner"; value: window.visibility === 4 ? 0 : 24 }
    Binding { target: Shell; property: "leading"; value: window.contentLeft }

    Component.onCompleted: {
        Chrome.setDark(Theme.dark)
        Chrome.attach(window)
        visible = true
    }
    Connections {
        target: Theme
        function onDarkChanged() { Chrome.setDark(Theme.dark) }
    }
    Connections {
        target: App
        function onPageChanged() { window.detailFold = App.page === window.detailPage || App.page === window.playerPage }
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
    Shortcut { sequence: "Ctrl+\\"; onActivated: window.toggleSidebar() }
    Shortcut {
        sequence: "Esc"
        enabled: App.page === window.detailPage && Object.keys(App.dialogs.current).length === 0
                 && Object.keys(App.detail.groupDialog).length === 0
        onActivated: App.closeDetail()
    }

    Rectangle { parent: window.contentItem; anchors.fill: parent; radius: Shell.corner; color: T.bg }

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

    // Sidebar
    Glass {
        id: sidebar
        visible: !window.immersive
        backdrop: content
        x: window.sidebarInset; y: window.sidebarInset
        width: window.sidebarShown
        fullWidth: 224
        height: parent.height - window.sidebarInset * 2
        HoverHandler { id: sidebarHover }
        Timer {
            interval: 140
            running: sidebarHover.hovered && window.rail && !window.peekWanted
            onTriggered: window.peekWanted = true
        }
        Timer {
            interval: 260
            running: window.peekWanted && !sidebarHover.hovered
            onTriggered: window.peekWanted = false
        }

        Rectangle {
            id: navThumb
            property Item target: null
            x: 8
            y: !target ? 0 : target.parent === sidebar ? target.y : navColumn.y + target.y
            width: sidebar.width - 16; height: 34
            radius: 10
            color: T.accentSoft
            visible: target !== null && App.page !== window.detailPage
            Behavior on y { enabled: navThumb.visible; NumberAnimation { duration: T.normal; easing.type: Easing.OutCubic } }
        }
        component NavButton: Rectangle {
            id: nav
            required property int page
            property string badge: ""
            x: 8
            width: sidebar.width - 16; height: 34
            radius: 10
            // The site list is a page of the preferences.
            readonly property bool selected: App.page === page || (page === window.settingsPage && App.page === window.sitesPage)
            color: hover.hovered && !selected ? T.fill : "transparent"
            onSelectedChanged: if (selected) navThumb.target = nav
            Component.onCompleted: if (selected) navThumb.target = nav
            Accessible.role: Accessible.Button
            Accessible.name: window.pageNames[page]
            Accessible.onPressAction: App.selectPage(page)
            activeFocusOnTab: true
            readonly property bool visualFocus: activeFocus
            Keys.onReturnPressed: App.selectPage(page)
            Keys.onEnterPressed: App.selectPage(page)
            Keys.onSpacePressed: App.selectPage(page)
            FocusRing { target: nav; corner: nav.radius }
            Icon {
                x: 13
                anchors.verticalCenter: parent.verticalCenter
                size: 18
                name: window.pageIcons[nav.page]
                tint: nav.selected ? T.accentText : T.text2
            }
            Text {
                x: 42
                anchors.verticalCenter: parent.verticalCenter
                opacity: window.labelAlpha
                visible: opacity > 0
                text: window.pageNames[nav.page]
                color: nav.selected ? T.accentText : T.text
                font.pixelSize: T.body
                font.weight: nav.selected ? Font.DemiBold : Font.Normal
            }
            Rectangle {
                anchors.right: parent.right; anchors.rightMargin: 2 + 7 * window.sidebarReveal
                y: 2 + ((parent.height - height) / 2 - 2) * window.sidebarReveal
                visible: nav.badge !== ""
                width: Math.max(18, count.implicitWidth + 11); height: 18; radius: 9
                color: T.accent
                Text { id: count; anchors.centerIn: parent; text: nav.badge; color: T.accentInk; font.pixelSize: T.footnote; font.weight: Font.Bold }
            }
            HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
            Tip { text: nav.page === 1 ? "Ctrl + K" : "" }
            TapHandler { onTapped: App.selectPage(nav.page) }
        }
        Item {
            id: brand
            y: 10
            width: parent.width; height: 34
            Image {
                objectName: "brandLogo"
                x: 13
                anchors.verticalCenter: parent.verticalCenter
                width: 34; height: 34
                source: "../assets/logo.png"
                sourceSize: Qt.size(Math.ceil(68 * Screen.devicePixelRatio), Math.ceil(68 * Screen.devicePixelRatio))
                fillMode: Image.PreserveAspectFit
                mipmap: true
                Accessible.role: Accessible.Graphic
                Accessible.name: "Miku Library"
            }
            Text {
                x: 50
                anchors.verticalCenter: parent.verticalCenter
                opacity: window.labelAlpha
                visible: opacity > 0
                text: "Miku Library"
                color: T.text
                font.pixelSize: T.body
                font.weight: Font.DemiBold
            }
            IconButton {
                anchors.right: parent.right; anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                opacity: window.labelAlpha
                visible: opacity > 0 && window.width >= 1060
                side: 28
                iconName: "sidebar"
                tint: T.text2
                tip: window.rail ? "固定侧栏 (Ctrl + \\)" : "收起侧栏 (Ctrl + \\)"
                focusPolicy: Qt.TabFocus
                onClicked: window.toggleSidebar()
            }
            DragHandler { target: null; onActiveChanged: if (active) window.startSystemMove() }
        }
        Column {
            id: navColumn
            y: brand.y + brand.height + 14
            width: parent.width
            spacing: 2
            Repeater {
                model: [0, 1, 2, 3, window.playerPage]
                NavButton {
                    required property int modelData
                    page: modelData
                    badge: page === window.downloadsPage && App.activeDownloads > 0 ? App.activeDownloads : ""
                }
            }
        }
        NavButton {
            id: settingsNav
            page: window.settingsPage
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 8
        }
    }

    // Thin glyphs need something behind them once posters pass underneath.
    Rectangle {
        objectName: "captionBacking"
        anchors.right: parent.right
        width: caption.width + 10
        height: caption.height
        color: T.bg
        topRightRadius: Shell.corner
        bottomLeftRadius: 16
        opacity: Shell.underCaption > 0 ? 1 : 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: T.fast; easing.type: Easing.OutCubic } }
    }

    // Window controls and the appearance menu, top right of every page.
    Row {
        id: caption
        visible: !window.immersive
        anchors.right: parent.right
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
            Tip { text: "打开下载页面" }
            TapHandler { onTapped: App.openNewVersion() }
        }
        IconButton {
            id: themeButton
            anchors.verticalCenter: parent.verticalCenter
            side: 30
            iconName: Theme.mode === "dark" ? "moon" : Theme.mode === "light" ? "sun" : "monitor"
            tip: "外观：" + (Theme.mode === "dark" ? "深色" : Theme.mode === "light" ? "浅色" : "跟随系统")
            focusPolicy: Qt.TabFocus
            onClicked: themeMenu.popup(themeButton, themeButton.width - 220, themeButton.height + 4)
            NxMenu {
                id: themeMenu
                NxMenuItem { text: "跟随系统"; iconName: "monitor"; checkable: true; checked: Theme.mode === "system"; onTriggered: App.settings.setAppearance("system") }
                NxMenuItem { text: "浅色"; iconName: "sun"; checkable: true; checked: Theme.mode === "light"; onTriggered: App.settings.setAppearance("light") }
                NxMenuItem { text: "深色"; iconName: "moon"; checkable: true; checked: Theme.mode === "dark"; onTriggered: App.settings.setAppearance("dark") }
            }
        }
        WindowButtons { window: window }
    }

    UpdatePanel { anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 20 }
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
}
