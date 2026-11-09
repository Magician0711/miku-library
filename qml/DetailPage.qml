// Miku Library - Copyright (C) 2026 Magician0711
// SPDX-License-Identifier: GPL-3.0-only
import QtQuick
import QtQuick.Templates as Tpl

// Wide windows read like a record sleeve: artwork and facts stay put on the left while
// the resources scroll on the right. Narrow windows stack the two.
Scaffold {
    id: page
    readonly property var detail: App.detail
    readonly property var hero: detail.hero
    readonly property real contentWidth: width - leading - trailing
    readonly property bool wide: restingWidth >= 960
    readonly property real sideWidth: wide ? Math.round(Math.min(340, Math.max(280, restingWidth * 0.26))) : 0
    readonly property real listLeft: wide ? leading + sideWidth + 32 : leading
    readonly property real listWidth: width - listLeft - trailing
    // Where the filter buttons decide whether they fit on one line.
    readonly property real restingListWidth: restingWidth - (wide ? sideWidth + 32 : 0)
    band: Shell.toolBand
    // Wide enough for the episode label at the largest text size.
    readonly property int pillWidth: Math.round(62 * Theme.textScale)
    scrolled: list.contentY - list.originY + page.band

    Connections {
        target: page.detail
        // A new anime starts at the top.
        function onHeroChanged() {
            if (page.shownId !== page.hero.subjectId) {
                page.shownId = page.hero.subjectId
                wheel.pinned = true
                list.settle()
                side.contentY = 0
            }
        }
    }
    property int shownId: 0

    // With no videos yet, finding some is the one thing to do here: the empty page offers it.
    readonly property bool offersSearch: detail.localOnly && detail.emptyVisible
    toolbar: NxButton {
        visible: !page.offersSearch
        text: page.detail.localOnly ? "查找可下载资源" : "重新搜索"
        iconName: page.detail.localOnly ? "search" : "refresh"
        onClicked: page.detail.researchClicked()
    }

    Backdrop {
        height: 560
        y: page.wide ? 0 : list.originY - list.contentY - page.band
        cover: page.hero.cover || ""
    }

    // Stays put: only the list on the right moves.
    fixed: Flickable {
        id: side
        x: page.leading
        y: page.band
        width: page.sideWidth
        height: parent.height - y
        visible: page.wide
        contentHeight: sideColumn.implicitHeight + 24
        interactive: false
        boundsBehavior: Flickable.StopAtBounds
        clip: true
        Tpl.ScrollBar.vertical: NxScrollBar {}
        SmoothWheel { view: side }
        Column {
            id: sideColumn
            width: page.sideWidth
            spacing: 18
            topPadding: 6
            HeroPoster {
                // As wide as the text under it, so the column has one right edge.
                width: parent.width
                height: Math.round(width * 1.414)
                cover: page.hero.cover || ""
            }
            HeroInfo { width: parent.width; titleSize: T.title1 }
        }
    }

    ListView {
        id: list
        anchors.fill: parent
        leftMargin: page.listLeft
        rightMargin: page.trailing
        topMargin: page.band
        model: page.detail.rows
        interactive: false
        boundsBehavior: Flickable.StopAtBounds
        cacheBuffer: 800
        header: headerBlock
        footer: footerBlock
        Tpl.ScrollBar.vertical: NxScrollBar { windowEdge: true }
        SmoothWheel { id: wheel; view: list }
        // A header that grows pushes the content upwards, out of view: stay with the top,
        // at once, or the frame being drawn shows the heading pushed up.
        function settle() { if (wheel.pinned) contentY = originY - topMargin }
        onOriginYChanged: { settle(); Qt.callLater(settle) }
        onTopMarginChanged: Qt.callLater(settle)
        // The view does not follow its own margin.
        onLeftMarginChanged: contentX = -leftMargin
        add: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: T.fast; easing.type: Easing.OutCubic } }
        remove: Transition { NumberAnimation { property: "opacity"; to: 0; duration: T.fast } }
        displaced: Transition { NumberAnimation { properties: "y"; duration: T.normal; easing.type: Easing.OutCubic } }
        delegate: FocusScope {
            id: row
            // Tabbing to a row that is off screen brings it into view.
            onActiveFocusChanged: if (activeFocus) list.positionViewAtIndex(index, ListView.Contain)
            required property int index
            required property string key
            required property string kind
            required property var group
            required property string text
            required property bool first
            required property bool last
            required property var expanded
            required property var bulkVisible
            required property var bulkEnabled
            required property var bulkBusy
            required property var bulkIcon
            required property var bulkText
            required property var bulkTip
            required property var headerTip
            required property var episode
            required property var batch
            required property var chips
            required property var meta
            required property var metaTip
            required property var sourcePage
            required property var copyVisible
            required property var copyEnabled
            required property var copyTip
            required property var actionText
            required property var actionIcon
            required property var actionPrimary
            required property var actionEnabled
            required property var actionTip

            readonly property int gapAbove: first && index > 0 ? 12 : 0
            width: page.listWidth
            height: gapAbove + loader.height
            Item {
                // Rows of a group draw one card between them: each shows its slice of a
                // rounded shape that continues past the row's edges.
                y: row.gapAbove
                width: parent.width
                height: loader.height
                clip: true
                visible: row.kind !== "moreGroups"
                Rectangle {
                    y: row.first ? 0 : -20
                    width: parent.width
                    height: parent.height + (row.first ? 0 : 20) + (row.last ? 0 : 20)
                    radius: 14
                    color: T.surface
                }
            }
            Loader {
                id: loader
                y: row.gapAbove
                width: parent.width
                sourceComponent: row.kind === "group" ? groupRow : row.kind === "release" ? releaseRow : row.kind === "moreRows" ? moreRow : moreGroupsRow
                property var rowData: row
            }
        }
    }

    // Back stays in reach however far the list has scrolled. It names where it leads, and
    // is drawn as plainly as a link: the page's own actions are the buttons.
    NxButton {
        objectName: "detailBack"
        parent: page.overlay
        // The chevron lines up with the artwork below.
        x: page.leading - leftPadding
        y: 8
        kind: "ghost"
        text: App.backTitle
        iconName: "chevron-left"
        iconSize: 18
        leftPadding: 6; rightPadding: 12
        tip: "返回" + App.backTitle + " (Esc)"
        onClicked: App.closeDetail()
        // In the stacked layout the rows pass underneath, and its own fill is see-through.
        Rectangle {
            z: -2
            anchors.fill: parent
            radius: height / 2
            color: T.bg
            visible: !page.wide && page.lift > 4
        }
    }

    Component {
        id: groupRow
        Item {
            readonly property var r: parent ? parent.rowData : null
            height: 48
            Item {
                anchors.fill: parent
                anchors.rightMargin: bulk.visible ? bulk.width + 20 : 0
                id: groupHeader
                activeFocusOnTab: true
                readonly property bool visualFocus: activeFocus
                Keys.onReturnPressed: page.detail.toggleGroup(r.group)
                Keys.onEnterPressed: page.detail.toggleGroup(r.group)
                Keys.onSpacePressed: page.detail.toggleGroup(r.group)
                FocusRing { target: groupHeader; corner: 10 }
                HoverHandler { id: headerHover; cursorShape: Qt.PointingHandCursor }
                Tip { text: r ? r.headerTip || "" : "" }
                TapHandler { onTapped: page.detail.toggleGroup(r.group) }
                Accessible.role: Accessible.Button
                Accessible.name: r ? r.text : ""
                Accessible.onPressAction: page.detail.toggleGroup(r.group)
                Row {
                    x: 14
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 8
                    Icon {
                        anchors.verticalCenter: parent.verticalCenter
                        size: 15
                        name: "chevron-right"
                        tint: T.text3
                        rotation: r && r.expanded ? 90 : 0
                        Behavior on rotation { NumberAnimation { duration: T.fast; easing.type: Easing.OutCubic } }
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: r ? r.text : ""
                        color: T.text
                        font.pixelSize: T.title3
                        font.weight: Font.DemiBold
                    }
                }
            }
            Item {
                id: bulk
                anchors.right: parent.right; anchors.rightMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                width: bulkButton.implicitWidth
                height: bulkButton.implicitHeight
                visible: r ? r.bulkVisible === true : false
                Tip { text: r ? r.bulkTip || "" : "" }
                NxButton {
                    id: bulkButton
                    objectName: "groupDownload"
                    kind: "soft"
                    text: r ? r.bulkText || "" : ""
                    iconName: r ? r.bulkIcon || "" : ""
                    busy: r ? r.bulkBusy === true : false
                    enabled: r ? r.bulkEnabled === true : false
                    Accessible.name: r ? r.group + " " + text : ""
                    onClicked: page.detail.askGroupDownload(r.group)
                }
            }
        }
    }
    Component {
        id: releaseRow
        Item {
            id: release
            readonly property var r: parent ? parent.rowData : null
            height: Math.max(middle.implicitHeight, 34) + 22
            HoverHandler { id: rowHover }
            Rectangle {
                anchors.fill: parent
                color: rowHover.hovered ? T.surface2 : "transparent"
                bottomLeftRadius: release.r && release.r.last ? 14 : 0
                bottomRightRadius: release.r && release.r.last ? 14 : 0
            }
            Divider { x: 14; width: parent.width - 14 }
            Rectangle {
                x: 14; y: 12
                width: page.pillWidth; height: 24; radius: 12
                color: release.r && release.r.batch ? T.accentSoft : T.fill
                Text {
                    anchors.centerIn: parent
                    width: parent.width - 8
                    horizontalAlignment: Text.AlignHCenter
                    text: release.r ? release.r.episode || "" : ""
                    color: release.r && release.r.batch ? T.accentText : T.text
                    font.pixelSize: T.callout
                    font.weight: Font.Medium
                    elide: Text.ElideRight
                }
            }
            Column {
                id: middle
                x: 26 + page.pillWidth; y: 11
                width: parent.width - x - buttons.width - 24
                spacing: 5
                BodyText {
                    width: parent.width
                    text: release.r ? release.r.text : ""
                    color: T.text
                }
                Flow {
                    width: parent.width
                    spacing: 4
                    visible: release.r && release.r.chips && release.r.chips.length > 0
                    Repeater {
                        model: release.r ? release.r.chips : []
                        Chip { required property var modelData; info: modelData }
                    }
                }
                Text {
                    width: parent.width
                    text: release.r ? release.r.meta || "" : ""
                    visible: text !== ""
                    color: T.text3
                    font.pixelSize: T.callout
                    wrapMode: Text.Wrap
                    Tip { text: release.r ? release.r.metaTip || "" : "" }
                }
            }
            Row {
                id: buttons
                anchors.right: parent.right; anchors.rightMargin: 10
                y: 9
                spacing: 2
                IconButton {
                    visible: release.r && !!release.r.sourcePage
                    iconName: "external"
                    tint: T.text2
                    tip: "打开发布页"
                    onClicked: page.detail.openPage(release.r.sourcePage)
                }
                IconButton {
                    visible: release.r && release.r.copyVisible === true
                    enabled: release.r && release.r.copyEnabled === true
                    iconName: "copy"
                    tint: T.text2
                    tip: release.r ? release.r.copyTip || "" : ""
                    onClicked: page.detail.copyLink(release.r.key)
                }
                Item { width: 4; height: 1 }
                NxButton {
                    implicitWidth: Math.max(78, implicitContentWidth + 26)
                    kind: release.r && release.r.actionPrimary ? "primary" : "soft"
                    text: release.r ? release.r.actionText || "" : ""
                    iconName: release.r ? release.r.actionIcon || "" : ""
                    enabled: release.r && release.r.actionEnabled === true
                    tip: release.r ? release.r.actionTip || "" : ""
                    onClicked: page.detail.releaseAction(release.r.key)
                }
            }
        }
    }
    Component {
        id: moreRow
        Item {
            id: moreItem
            readonly property var r: parent ? parent.rowData : null
            height: 40
            activeFocusOnTab: true
            readonly property bool visualFocus: activeFocus
            Keys.onReturnPressed: page.detail.toggleShowAll(r.group)
            Keys.onEnterPressed: page.detail.toggleShowAll(r.group)
            Keys.onSpacePressed: page.detail.toggleShowAll(r.group)
            Accessible.role: Accessible.Button
            Accessible.name: r ? r.text : ""
            Accessible.onPressAction: page.detail.toggleShowAll(r.group)
            FocusRing { target: moreItem; corner: 10 }
            Divider { x: 14; width: parent.width - 14 }
            Text {
                anchors.centerIn: parent
                text: r ? r.text : ""
                color: T.accentText
                font.pixelSize: T.body
                font.weight: Font.Medium
            }
            HoverHandler { cursorShape: Qt.PointingHandCursor }
            TapHandler { onTapped: page.detail.toggleShowAll(r.group) }
        }
    }
    Component {
        id: moreGroupsRow
        Item {
            readonly property var r: parent ? parent.rowData : null
            height: 40
            NxButton {
                anchors.centerIn: parent
                text: r ? r.text : ""
                iconName: "chevron-down"
                onClicked: page.detail.showAllGroups()
            }
        }
    }
    Component {
        id: footerBlock
        Item {
            width: page.listWidth
            height: page.detail.emptyVisible ? (page.offersSearch ? 330 : 280) : 24
            EmptyState {
                anchors.fill: parent
                iconName: "search"
                title: page.detail.emptyTitle
                description: page.detail.emptyText
                shown: page.detail.emptyVisible
                actionText: page.offersSearch ? "查找可下载资源" : ""
                actionIcon: "search"
                actionKind: "primary"
                onAction: page.detail.researchClicked()
            }
        }
    }
    Component {
        id: headerBlock
        Column {
            id: head
            width: page.listWidth
            topPadding: 6
            bottomPadding: 14
            spacing: 14
            readonly property var filters: page.detail.filters

            Item {
                // Stacked layout: artwork first, as on a phone-width window.
                visible: !page.wide
                width: parent.width
                height: visible ? Math.max(art.height, stacked.implicitHeight) + 4 : 0
                HeroPoster { id: art; width: 180; height: 254; cover: page.hero.cover || "" }
                HeroInfo { id: stacked; x: art.width + 22; width: parent.width - x; summaryLines: 3 }
            }
            Item {
                width: parent.width
                height: 32
                Row {
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 10
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: page.detail.localOnly ? "本地视频" : "可用资源"
                        color: T.text
                        font.pixelSize: T.title1
                        font.weight: Font.Bold
                    }
                    Text { anchors.verticalCenter: parent.verticalCenter; text: page.detail.count; color: T.text2; font.pixelSize: T.callout }
                }
                NxSelect {
                    anchors.right: parent.right
                    implicitWidth: 124 * Theme.textScale
                    tip: "排序"
                    model: [{ text: "推荐", value: "popular" }, { text: "最近更新", value: "latest" }, { text: "做种最多", value: "seeders" },
                            { text: "资源最多", value: "count" }, { text: "名称", value: "name" }]
                    value: head.filters.sort
                    onChosen: value => page.detail.setFilter("sort", value)
                }
            }
            Flow {
                width: parent.width
                spacing: 12
                visible: !page.detail.localOnly
                Text { text: page.detail.progress; color: T.text; font.pixelSize: T.callout; font.weight: Font.DemiBold }
                Repeater {
                    model: page.detail.sources
                    Row {
                        id: source
                        required property var modelData
                        spacing: 5
                        Accessible.role: Accessible.StaticText
                        Accessible.name: modelData.name + " " + ({ ok: "有结果", warn: "没有结果", err: "连接失败", busy: "检索中" })[modelData.state]
                        Dot { anchors.verticalCenter: parent.verticalCenter; status: source.modelData.state }
                        Text { text: source.modelData.name; color: T.text2; font.pixelSize: T.callout; Tip { text: source.modelData.tip } }
                    }
                }
            }
            Group {
                width: parent.width
                height: filterColumn.implicitHeight + 24
                Column {
                    id: filterColumn
                    x: 12; y: 12
                    width: parent.width - 24
                    spacing: 8
                    Flow {
                        id: filters
                        width: parent.width
                        spacing: 6
                        // While the sidebar passes the buttons give way a little, instead of
                        // the last one dropping to a new line and back. A window too narrow
                        // for one line even at rest wraps as usual.
                        readonly property real unit: 124 * Theme.textScale
                        readonly property real needed: unit * 4 + (page.detail.series.length > 1 ? unit * 200 / 124 : unit)
                        readonly property real squeeze: page.restingListWidth - 24 >= needed + 24
                            ? Math.min(1, (width - 25) / needed) : 1
                        NxSelect {
                            // Unlike the filters beside it, this moves the whole page.
                            objectName: "seriesSelect"
                            visible: page.detail.series.length > 1
                            implicitWidth: filters.unit * 200 / 124 * filters.squeeze
                            popupWidth: 340
                            tip: "切换到同名番的其他季度或篇目"
                            model: page.detail.series
                            value: page.hero.subjectId
                            onChosen: value => page.detail.switchSeries(value)
                        }
                        NxSelect {
                            // With a series the page already shows one season at a time.
                            visible: page.detail.series.length < 2
                            implicitWidth: filters.unit * filters.squeeze
                            tip: "放送季度"
                            model: page.detail.seasons
                            value: head.filters.season
                            onChosen: value => page.detail.setFilter("season", value)
                        }
                        NxSelect {
                            implicitWidth: filters.unit * filters.squeeze
                            tip: "资源类型"
                            model: [{ text: "全部类型", value: "all" }, { text: "合集", value: "batch" }, { text: "单集", value: "single" }]
                            value: head.filters.kind
                            onChosen: value => page.detail.setFilter("kind", value)
                        }
                        NxSelect {
                            implicitWidth: filters.unit * filters.squeeze
                            tip: "清晰度"
                            model: [{ text: "全部清晰度", value: "" }, { text: "4K", value: "2160p" }, { text: "1080p", value: "1080p" }, { text: "720p", value: "720p" }]
                            value: head.filters.resolution
                            onChosen: value => page.detail.setFilter("resolution", value)
                        }
                        NxSelect {
                            implicitWidth: filters.unit * filters.squeeze
                            tip: "字幕语言"
                            model: [{ text: "全部字幕", value: "" }, { text: "中文", value: "zh" }, { text: "简体", value: "zh-Hans" },
                                    { text: "繁体", value: "zh-Hant" }, { text: "英文", value: "en" }]
                            value: head.filters.subtitle
                            onChosen: value => page.detail.setFilter("subtitle", value)
                        }
                        NxSelect {
                            implicitWidth: filters.unit * filters.squeeze
                            tip: "下载状态"
                            enabled: !page.detail.localOnly
                            model: [{ text: "全部状态", value: "all" }, { text: "已下载", value: "downloaded" }, { text: "未下载", value: "missing" }]
                            value: head.filters.status
                            onChosen: value => page.detail.setFilter("status", value)
                        }
                    }
                    Item {
                        width: parent.width
                        height: 32
                        NxField {
                            width: parent.width - hdr.width - reset.width - 20
                            iconName: "filter"
                            clearable: true
                            placeholderText: "在结果中筛选标题关键词…"
                            text: head.filters.text
                            onTextEdited: page.detail.setFilter("text", text)
                            onTextChanged: if (text !== head.filters.text) page.detail.setFilter("text", text)
                        }
                        NxSwitch {
                            id: hdr
                            anchors.right: reset.left; anchors.rightMargin: 10
                            anchors.verticalCenter: parent.verticalCenter
                            text: "仅 HDR"
                            checked: head.filters.hdrOnly === true
                            onToggled: page.detail.setFilter("hdrOnly", checked)
                        }
                        NxButton {
                            id: reset
                            anchors.right: parent.right
                            kind: "ghost"
                            text: "重置"
                            tip: "重置筛选"
                            onClicked: page.detail.resetFilters()
                        }
                    }
                }
            }
        }
    }
}
